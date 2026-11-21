#include "motion_detector.h"
#include <string.h>

namespace {
	// How fast the background follows the frames: a quarter of each still frame, so the light of the day blends in within a few seconds
	constexpr uint16_t STILL_BLEND_WEIGHT = 4;

	// A sixteenth of each frame with motion, so a bird that sits still for a while blends in too, but a passing one doesn't
	constexpr uint16_t MOTION_BLEND_WEIGHT = 16;

	// Half of each frame while warming up, so the background catches up with the exposure quickly
	constexpr uint16_t WARMUP_BLEND_WEIGHT = 2;

	// Average brightness of a frame
	int32_t getAverage(const uint8_t *pixels, size_t pixelCount) {
		uint32_t sum = 0;
		for (size_t index = 0; index < pixelCount; index++) {
			sum += pixels[index];
		}
		return static_cast<int32_t>(sum / pixelCount);
	}

	// Blend a frame into the background, with the given weight (the background keeps weight - 1 parts)
	void blend(uint8_t *background, const uint8_t *frame, size_t pixelCount, uint16_t weight) {
		for (size_t index = 0; index < pixelCount; index++) {
			const uint32_t mixed = static_cast<uint32_t>(background[index]) * (weight - 1) + frame[index] + weight / 2;
			background[index] = static_cast<uint8_t>(mixed / weight);
		}
	}
}

// Forget the background
void MotionDetector::reset() {
	width = 0;
	height = 0;
	learnedFrames = 0;
}

// Learn a frame as the whole background
void MotionDetector::learn(const uint8_t *frame, uint16_t frameWidth, uint16_t frameHeight) {
	width = frameWidth;
	height = frameHeight;
	memcpy(background, frame, static_cast<size_t>(frameWidth) * frameHeight);
	learnedFrames = 1;
}

// Compare a frame with the background, then blend it in
MotionResult MotionDetector::update(const uint8_t *frame, uint16_t frameWidth, uint16_t frameHeight, const MotionThresholds &thresholds) {
	// Refuse empty frames and frames bigger than the background
	if (frame == nullptr || frameWidth == 0 || frameHeight == 0 || frameWidth > MOTION_FRAME_MAX_WIDTH || frameHeight > MOTION_FRAME_MAX_HEIGHT) {
		reset();
		return { false, 0 };
	}

	// Start over on the first frame, or a frame of another size
	if (learnedFrames == 0 || frameWidth != width || frameHeight != height) {
		learn(frame, frameWidth, frameHeight);
		return { false, 0 };
	}

	// Remove the change of the whole picture first (a cloud, the sensor's exposure): only what changes against the rest counts
	const size_t pixelCount = static_cast<size_t>(width) * height;
	const int32_t brightnessShift = getAverage(frame, pixelCount) - getAverage(background, pixelCount);

	// Count the pixels that changed more than the threshold
	size_t changedPixels = 0;
	for (size_t index = 0; index < pixelCount; index++) {
		int32_t difference = static_cast<int32_t>(frame[index]) - brightnessShift - background[index];
		if (difference < 0) {
			difference = -difference;
		}
		if (difference > thresholds.pixelDelta) {
			changedPixels++;
		}
	}
	const uint8_t changedPercent = static_cast<uint8_t>(changedPixels * 100 / pixelCount);

	// While warming up, only learn
	if (learnedFrames < MOTION_WARMUP_FRAMES) {
		blend(background, frame, pixelCount, WARMUP_BLEND_WEIGHT);
		learnedFrames++;
		return { false, changedPercent };
	}

	// Report the motion, and blend the frame in (slowly when something moved)
	const bool isMotion = thresholds.changedPercent > 0 && changedPercent >= thresholds.changedPercent;
	blend(background, frame, pixelCount, isMotion ? MOTION_BLEND_WEIGHT : STILL_BLEND_WEIGHT);
	return { isMotion, changedPercent };
}

// Turn RGB888 pixels into grayscale ones
void convertRgbToGray(const uint8_t *rgb, size_t pixelCount, uint8_t *gray) {
	for (size_t index = 0; index < pixelCount; index++) {
		const uint8_t *pixel = rgb + index * 3;
		gray[index] = static_cast<uint8_t>((pixel[0] + 2 * pixel[1] + pixel[2] + 2) / 4);
	}
}
