#include "birdhouse_settings.h"
#include <string.h>
#include "../camera/capture_options.h"

namespace {
	// Names of the sensitivities in the web API, in the order of Sensitivity
	constexpr const char *SENSITIVITY_NAMES[] = { "low", "medium", "high" };

	// Thresholds of each sensitivity, in the order of Sensitivity (a bird close to the camera changes a big part of the picture, a shadow or a moving leaf a small one)
	constexpr MotionThresholds SENSITIVITY_THRESHOLDS[] = {
		{ 40, 12 },
		{ 30, 6 },
		{ 20, 3 }
	};

	// Check that a field, when present, is a boolean
	bool isValidBoolean(JsonVariantConst value) {
		return value.isNull() || value.is<bool>();
	}

	// Check that a field, when present, is a sensitivity name
	bool isValidSensitivity(JsonVariantConst value) {
		Sensitivity sensitivity;
		return value.isNull() || (value.is<const char *>() && findSensitivity(value.as<const char *>(), sensitivity));
	}

	// Check that a field, when present, is a whole number of seconds in range
	bool isValidCooldown(JsonVariantConst value) {
		if (value.isNull()) {
			return true;
		}
		if (!value.is<int>()) {
			return false;
		}
		const int seconds = value.as<int>();
		return seconds >= COOLDOWN_SECONDS_MIN && seconds <= COOLDOWN_SECONDS_MAX;
	}

	// Check that a field, when present, is a resolution the sensor can take
	bool isValidResolution(JsonVariantConst value, size_t maxResolutionIndex) {
		if (value.isNull()) {
			return true;
		}
		if (!value.is<const char *>()) {
			return false;
		}
		const int resolutionIndex = findResolutionIndex(value.as<const char *>());
		return resolutionIndex != RESOLUTION_NOT_FOUND && static_cast<size_t>(resolutionIndex) <= maxResolutionIndex;
	}
}

// The settings of a new birdhouse
BirdhouseSettings getDefaultSettings() {
	return { true, Sensitivity::Medium, COOLDOWN_SECONDS_DEFAULT, static_cast<size_t>(findResolutionIndex(PHOTO_RESOLUTION_DEFAULT)), false };
}

// The thresholds of a sensitivity
MotionThresholds getMotionThresholds(Sensitivity sensitivity) {
	return SENSITIVITY_THRESHOLDS[static_cast<uint8_t>(sensitivity)];
}

// The web API name of a sensitivity
const char *getSensitivityName(Sensitivity sensitivity) {
	return SENSITIVITY_NAMES[static_cast<uint8_t>(sensitivity)];
}

// Find a sensitivity by its web API name
bool findSensitivity(const char *name, Sensitivity &sensitivity) {
	for (uint8_t index = 0; index < sizeof(SENSITIVITY_NAMES) / sizeof(SENSITIVITY_NAMES[0]); index++) {
		if (strcmp(SENSITIVITY_NAMES[index], name) == 0) {
			sensitivity = static_cast<Sensitivity>(index);
			return true;
		}
	}
	return false;
}

// Write the settings to a JSON object (the texts are literals, so ArduinoJson can keep pointers to them)
void writeSettingsToJson(JsonObject json, const BirdhouseSettings &settings) {
	json["detectionEnabled"] = settings.isDetectionEnabled;
	json["sensitivity"] = getSensitivityName(settings.sensitivity);
	json["cooldownSeconds"] = settings.cooldownSeconds;
	json["photoResolution"] = RESOLUTIONS[settings.photoResolutionIndex].name;
	json["flash"] = settings.useFlash;
}

// Apply the settings present in a JSON object, once every field is valid
bool applySettingsFromJson(JsonObjectConst json, size_t maxResolutionIndex, BirdhouseSettings &settings, const char *&invalidField) {
	// Check every field first, so a refused request changes nothing
	if (!isValidBoolean(json["detectionEnabled"])) {
		invalidField = "detectionEnabled";
		return false;
	}
	if (!isValidSensitivity(json["sensitivity"])) {
		invalidField = "sensitivity";
		return false;
	}
	if (!isValidCooldown(json["cooldownSeconds"])) {
		invalidField = "cooldownSeconds";
		return false;
	}
	if (!isValidResolution(json["photoResolution"], maxResolutionIndex)) {
		invalidField = "photoResolution";
		return false;
	}
	if (!isValidBoolean(json["flash"])) {
		invalidField = "flash";
		return false;
	}

	// Apply the fields the request has
	if (!json["detectionEnabled"].isNull()) {
		settings.isDetectionEnabled = json["detectionEnabled"].as<bool>();
	}
	if (!json["sensitivity"].isNull()) {
		findSensitivity(json["sensitivity"].as<const char *>(), settings.sensitivity);
	}
	if (!json["cooldownSeconds"].isNull()) {
		settings.cooldownSeconds = static_cast<uint16_t>(json["cooldownSeconds"].as<int>());
	}
	if (!json["photoResolution"].isNull()) {
		settings.photoResolutionIndex = static_cast<size_t>(findResolutionIndex(json["photoResolution"].as<const char *>()));
	}
	if (!json["flash"].isNull()) {
		settings.useFlash = json["flash"].as<bool>();
	}
	return true;
}
