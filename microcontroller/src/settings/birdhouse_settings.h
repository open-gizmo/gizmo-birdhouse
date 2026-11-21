#ifndef BIRDHOUSE_SETTINGS_H
#define BIRDHOUSE_SETTINGS_H

#include <ArduinoJson.h>
#include <stddef.h>
#include <stdint.h>

// Settings of the visit detection, kept free of hardware headers so the native tests can use them

// How small a change in the picture counts as a visit
enum class Sensitivity : uint8_t {
	Low,
	Medium,
	High
};

// Seconds to wait after a visit photo before the next one, so a bird that stays doesn't fill the card
constexpr uint16_t COOLDOWN_SECONDS_MIN = 5;
constexpr uint16_t COOLDOWN_SECONDS_MAX = 3600;
constexpr uint16_t COOLDOWN_SECONDS_DEFAULT = 30;

// Default resolution of the visit photos (SXGA, 1280 x 1024: sharp, and about 150 KB on the card)
constexpr const char *PHOTO_RESOLUTION_DEFAULT = "SXGA";

// Settings of the birdhouse
struct BirdhouseSettings {
	bool isDetectionEnabled; // Take a photo when a bird comes in
	Sensitivity sensitivity;
	uint16_t cooldownSeconds;
	size_t photoResolutionIndex; // Index in RESOLUTIONS
	bool useFlash; // Light the flash LED for the visit photos (off by default: it can scare the birds)
};

// Thresholds the motion detector uses for a sensitivity
struct MotionThresholds {
	uint8_t pixelDelta; // How much a pixel must change (0 to 255) to count as changed
	uint8_t changedPercent; // How much of the picture must change to count as a visit
};

// The settings of a new birdhouse
BirdhouseSettings getDefaultSettings();

// The thresholds of a sensitivity
MotionThresholds getMotionThresholds(Sensitivity sensitivity);

// The web API name of a sensitivity ("low", "medium", "high"), and back (false for an unknown name)
const char *getSensitivityName(Sensitivity sensitivity);
bool findSensitivity(const char *name, Sensitivity &sensitivity);

// Write the settings to a JSON object
void writeSettingsToJson(JsonObject json, const BirdhouseSettings &settings);

// Apply the settings present in a JSON object (missing fields keep their current value): on an invalid field, nothing changes, and invalidField names it
bool applySettingsFromJson(JsonObjectConst json, size_t maxResolutionIndex, BirdhouseSettings &settings, const char *&invalidField);

#endif
