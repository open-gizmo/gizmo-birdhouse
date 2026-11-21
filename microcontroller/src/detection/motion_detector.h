#ifndef MOTION_DETECTOR_H
#define MOTION_DETECTOR_H

#include <stddef.h>
#include <stdint.h>
#include "../settings/birdhouse_settings.h"

// Motion detection on small grayscale frames, kept free of hardware headers so the native tests can use it

// Largest frame the detector compares (a QVGA frame decoded at 1/8 is 40 x 30)
constexpr uint16_t MOTION_FRAME_MAX_WIDTH = 80;
constexpr uint16_t MOTION_FRAME_MAX_HEIGHT = 60;
constexpr size_t MOTION_FRAME_MAX_PIXELS = static_cast<size_t>(MOTION_FRAME_MAX_WIDTH) * MOTION_FRAME_MAX_HEIGHT;

// Frames to learn the background from before reporting motion (the exposure of the sensor settles meanwhile)
constexpr uint8_t MOTION_WARMUP_FRAMES = 3;

// What the detector saw in a frame
struct MotionResult {
	bool isMotion;
	uint8_t changedPercent; // How much of the picture changed, from 0 to 100
};

// Compares each frame with a background that follows slow changes (the light of the day), and reports the fast ones (a bird coming in)
class MotionDetector {
public:
	// Forget the background (the next frames learn it again)
	void reset();

	// Compare a grayscale frame (one byte per pixel, row by row) with the background, then blend it in: a frame of another size, or bigger than the largest, starts over
	MotionResult update(const uint8_t *frame, uint16_t width, uint16_t height, const MotionThresholds &thresholds);

private:
	uint8_t background[MOTION_FRAME_MAX_PIXELS] = {};
	uint16_t width = 0;
	uint16_t height = 0;
	uint8_t learnedFrames = 0;

	// Learn a frame as the whole background
	void learn(const uint8_t *frame, uint16_t frameWidth, uint16_t frameHeight);
};

// Turn RGB888 pixels into grayscale ones (the channel order doesn't matter: green weighs most and sits in the middle)
void convertRgbToGray(const uint8_t *rgb, size_t pixelCount, uint8_t *gray);

#endif
