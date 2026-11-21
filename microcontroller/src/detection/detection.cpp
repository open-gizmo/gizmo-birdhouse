#include "detection.h"
#include <Arduino.h>
#include <atomic>
#include <jpeg_decoder.h>
#include "motion_detector.h"
#include "../camera/camera.h"
#include "../clock/clock.h"
#include "../settings/settings_storage.h"
#include "../visits/visit_store.h"
#include "../serial/serial.h"

namespace {
	// Time between two frames of the detection (a bird stays at the entrance for a few seconds)
	constexpr uint32_t DETECTION_INTERVAL_MS = 700;

	// Time between two cleanups of the visits deleted all at once
	constexpr uint32_t PURGE_INTERVAL_MS = 30000;

	// The detection frames: QVGA (320 x 240), decoded at 1/8 into 40 x 30 pixels, with a quality that keeps them small (they are also the small pictures of the visits)
	constexpr const char *DETECTION_RESOLUTION = "QVGA";
	constexpr uint8_t DETECTION_QUALITY = 12;

	// Stack and priority of the task (the same priority as the main loop, which only checks the Wi-Fi)
	constexpr uint32_t TASK_STACK_SIZE = 6144;
	constexpr UBaseType_t TASK_PRIORITY = 1;
	constexpr BaseType_t TASK_CORE = 1;

	// The detector, and the decoded frame it compares (RGB888, then grayscale)
	MotionDetector *detector = nullptr;
	uint8_t *rgbFrame = nullptr;
	uint8_t grayFrame[MOTION_FRAME_MAX_PIXELS];
	constexpr size_t RGB_FRAME_SIZE = MOTION_FRAME_MAX_PIXELS * 3;

	// Status, read by the web server task
	std::atomic<bool> isActive(false);
	std::atomic<uint32_t> lastMotionPercent(0);

	// When the last visit photo was taken (to wait for the cooldown)
	bool hasTakenVisit = false;
	uint32_t lastVisitAtMs = 0;

	// Decode a JPEG frame at 1/8 into grayscale pixels (false when it doesn't fit, or isn't a JPEG)
	bool decodeFrame(const camera_fb_t *frame, uint16_t &width, uint16_t &height) {
		esp_jpeg_image_cfg_t config = {};
		config.indata = frame->buf;
		config.indata_size = frame->len;
		config.outbuf = rgbFrame;
		config.outbuf_size = RGB_FRAME_SIZE;
		config.out_format = JPEG_IMAGE_FORMAT_RGB888;
		config.out_scale = JPEG_IMAGE_SCALE_1_8;

		// Decode the frame, then turn it into grayscale
		esp_jpeg_image_output_t output = {};
		if (esp_jpeg_decode(&config, &output) != ESP_OK || output.width > MOTION_FRAME_MAX_WIDTH || output.height > MOTION_FRAME_MAX_HEIGHT) {
			return false;
		}
		convertRgbToGray(rgbFrame, static_cast<size_t>(output.width) * output.height, grayFrame);
		width = output.width;
		height = output.height;
		return true;
	}

	// Whether the cooldown after the last visit photo is over
	bool isCooldownOver(const BirdhouseSettings &settings) {
		return !hasTakenVisit || millis() - lastVisitAtMs >= static_cast<uint32_t>(settings.cooldownSeconds) * 1000;
	}

	// Take the visit photo and save it with the frame the bird was seen in
	void saveVisitPhoto(const BirdhouseSettings &settings, const uint8_t *thumbnail, size_t thumbnailSize) {
		// Take the photo (a live snapshot from the web app may hold the camera: the visit waits for the next frame then)
		const CaptureOptions options = { settings.photoResolutionIndex, JPEG_QUALITY_DEFAULT, settings.useFlash };
		camera_fb_t *photo = nullptr;
		uint32_t captureTimeMs = 0;
		if (takeSnapshot(options, photo, captureTimeMs, false) != SnapshotResult::Ok) {
			return;
		}

		// Save it, then give the frame back to the camera
		VisitEntry entry;
		const VisitStoreResult result = saveVisit(photo->buf, photo->len, thumbnail, thumbnailSize, getUnixTime(), entry);
		releaseSnapshot(photo);

		// Start the cooldown (after a failure too, so a full or broken card isn't written every frame)
		hasTakenVisit = true;
		lastVisitAtMs = millis();
		if (result == VisitStoreResult::Ok) {
			printLogfln("Visit %lu saved: %lu bytes", static_cast<unsigned long>(entry.id), static_cast<unsigned long>(entry.sizeBytes));
		} else {
			printLogln("Failed to save the visit");
		}
	}

	// Look at one frame, and save a visit when a bird came in
	void runDetectionStep() {
		// Watch only when switched on, with a camera and a card to save the photos on
		const BirdhouseSettings settings = getSettings();
		const bool canDetect = settings.isDetectionEnabled && getCameraInfo().isAvailable && isStorageAvailable();
		isActive = canDetect;
		if (!canDetect) {
			detector->reset();
			lastMotionPercent = 0;
			return;
		}

		// Take a small frame (a live snapshot from the web app may hold the camera: this frame is skipped then)
		const CaptureOptions options = { static_cast<size_t>(findResolutionIndex(DETECTION_RESOLUTION)), DETECTION_QUALITY, false };
		camera_fb_t *frame = nullptr;
		uint32_t captureTimeMs = 0;
		if (takeSnapshot(options, frame, captureTimeMs, false) != SnapshotResult::Ok) {
			return;
		}

		// Compare it with the background
		uint16_t width = 0;
		uint16_t height = 0;
		MotionResult result = { false, 0 };
		if (decodeFrame(frame, width, height)) {
			result = detector->update(grayFrame, width, height, getMotionThresholds(settings.sensitivity));
		}
		lastMotionPercent = result.changedPercent;

		// Nothing came in, or a photo was taken a moment ago
		if (!result.isMotion || !isCooldownOver(settings)) {
			releaseSnapshot(frame);
			return;
		}

		// Keep the frame as the small picture of the visit (it shows the moment the bird was seen), and give the camera back for the photo
		printLogfln("Motion: %u%% of the picture changed", static_cast<unsigned>(result.changedPercent));
		uint8_t *thumbnail = static_cast<uint8_t *>(malloc(frame->len));
		const size_t thumbnailSize = thumbnail != nullptr ? frame->len : 0;
		if (thumbnail != nullptr) {
			memcpy(thumbnail, frame->buf, frame->len);
		}
		releaseSnapshot(frame);

		// Take and save the photo
		saveVisitPhoto(settings, thumbnail, thumbnailSize);
		free(thumbnail);
	}

	// Watch the camera, and clean up the deleted visits now and then
	void detectionTask(void *) {
		uint32_t lastPurgeAtMs = 0;
		purgeDeletedVisits(); // The visits deleted before a restart
		for (;;) {
			vTaskDelay(pdMS_TO_TICKS(DETECTION_INTERVAL_MS));
			runDetectionStep();
			if (millis() - lastPurgeAtMs >= PURGE_INTERVAL_MS) {
				purgeDeletedVisits();
				lastPurgeAtMs = millis();
			}
		}
	}
}

// Start the task that watches the camera
void setupDetection() {
	// The detector and the decoded frame (in PSRAM: they are a few KB each)
	detector = new MotionDetector();
	rgbFrame = static_cast<uint8_t *>(malloc(RGB_FRAME_SIZE));
	if (detector == nullptr || rgbFrame == nullptr) {
		printLogln("Not enough memory for the visit detection");
		return;
	}

	// Start the task
	if (xTaskCreatePinnedToCore(detectionTask, "detection", TASK_STACK_SIZE, nullptr, TASK_PRIORITY, nullptr, TASK_CORE) != pdPASS) {
		printLogln("Failed to start the visit detection");
	}
}

// What the visit detection is doing
DetectionStatus getDetectionStatus() {
	return { isActive, lastMotionPercent };
}
