#ifndef DETECTION_H
#define DETECTION_H

#include <stdint.h>

// What the visit detection is doing
struct DetectionStatus {
	bool isActive; // Watching for birds: switched on, with a camera and a card to save the photos on
	uint32_t lastMotionPercent; // How much of the picture changed in the last frame, from 0 to 100
};

// Start the task that watches the camera and saves a photo when a bird comes in (call after the camera, the settings and the card)
void setupDetection();

// What the visit detection is doing
DetectionStatus getDetectionStatus();

#endif
