#include "settings_storage.h"
#include <Arduino.h>
#include <Preferences.h>
#include "../serial/serial.h"

namespace {
	// NVS namespaces and keys (both are limited to 15 characters): the settings, and the visit counter apart, so a reset of the settings keeps counting
	constexpr char SETTINGS_NAMESPACE[] = "gizmo-birdhouse"; // The project name
	constexpr char COUNTER_NAMESPACE[] = "birdhouse-count";
	constexpr char DETECTION_KEY[] = "detection";
	constexpr char SENSITIVITY_KEY[] = "sensitivity";
	constexpr char COOLDOWN_KEY[] = "cooldown";
	constexpr char RESOLUTION_KEY[] = "resolution";
	constexpr char FLASH_KEY[] = "flash";
	constexpr char LAST_VISIT_ID_KEY[] = "lastVisitId";

	// Current settings, read by the detection task and written by the web server task
	BirdhouseSettings currentSettings = getDefaultSettings();
	portMUX_TYPE settingsLock = portMUX_INITIALIZER_UNLOCKED;

	// Use new settings
	void setSettings(const BirdhouseSettings &settings) {
		portENTER_CRITICAL(&settingsLock);
		currentSettings = settings;
		portEXIT_CRITICAL(&settingsLock);
	}
}

// Load the saved settings, clamped to what the sensor can take
void setupSettings(size_t maxResolutionIndex) {
	BirdhouseSettings settings = getDefaultSettings();

	// Read the saved settings on top of the defaults (missing keys keep their default)
	Preferences preferences;
	if (preferences.begin(SETTINGS_NAMESPACE, true)) {
		settings.isDetectionEnabled = preferences.getBool(DETECTION_KEY, settings.isDetectionEnabled);
		const uint8_t sensitivity = preferences.getUChar(SENSITIVITY_KEY, static_cast<uint8_t>(settings.sensitivity));
		if (sensitivity <= static_cast<uint8_t>(Sensitivity::High)) {
			settings.sensitivity = static_cast<Sensitivity>(sensitivity);
		}
		const uint16_t cooldownSeconds = preferences.getUShort(COOLDOWN_KEY, settings.cooldownSeconds);
		if (cooldownSeconds >= COOLDOWN_SECONDS_MIN && cooldownSeconds <= COOLDOWN_SECONDS_MAX) {
			settings.cooldownSeconds = cooldownSeconds;
		}
		settings.photoResolutionIndex = preferences.getUChar(RESOLUTION_KEY, static_cast<uint8_t>(settings.photoResolutionIndex));
		settings.useFlash = preferences.getBool(FLASH_KEY, settings.useFlash);
		preferences.end();
		printLogln("Settings loaded");
	} else {
		printLogln("No saved settings, using the defaults");
	}

	// Never take photos at a resolution the sensor can't (a smaller sensor fitted since)
	if (settings.photoResolutionIndex > maxResolutionIndex) {
		settings.photoResolutionIndex = maxResolutionIndex;
	}
	setSettings(settings);
}

// The current settings
BirdhouseSettings getSettings() {
	portENTER_CRITICAL(&settingsLock);
	const BirdhouseSettings settings = currentSettings;
	portEXIT_CRITICAL(&settingsLock);
	return settings;
}

// Save the settings and use them
bool saveSettings(const BirdhouseSettings &settings) {
	Preferences preferences;
	if (!preferences.begin(SETTINGS_NAMESPACE, false)) {
		return false;
	}

	// Write every setting, checking each write (NVS skips values that didn't change)
	bool isSaved = true;
	isSaved &= preferences.putBool(DETECTION_KEY, settings.isDetectionEnabled) > 0;
	isSaved &= preferences.putUChar(SENSITIVITY_KEY, static_cast<uint8_t>(settings.sensitivity)) > 0;
	isSaved &= preferences.putUShort(COOLDOWN_KEY, settings.cooldownSeconds) > 0;
	isSaved &= preferences.putUChar(RESOLUTION_KEY, static_cast<uint8_t>(settings.photoResolutionIndex)) > 0;
	isSaved &= preferences.putBool(FLASH_KEY, settings.useFlash) > 0;
	preferences.end();

	// Use them once saved
	if (isSaved) {
		setSettings(settings);
	}
	printLogln(isSaved ? "Settings saved" : "Failed to save the settings");
	return isSaved;
}

// Forget the saved settings
bool clearSettings() {
	Preferences preferences;
	if (!preferences.begin(SETTINGS_NAMESPACE, false)) {
		return false;
	}
	const bool isCleared = preferences.clear();
	preferences.end();
	return isCleared;
}

// The number of the last visit taken
uint32_t getLastVisitId() {
	Preferences preferences;
	if (!preferences.begin(COUNTER_NAMESPACE, true)) {
		return 0;
	}
	const uint32_t id = preferences.getULong(LAST_VISIT_ID_KEY, 0);
	preferences.end();
	return id;
}

// Save the number of the last visit taken
void saveLastVisitId(uint32_t id) {
	Preferences preferences;
	if (!preferences.begin(COUNTER_NAMESPACE, false)) {
		return;
	}
	preferences.putULong(LAST_VISIT_ID_KEY, id);
	preferences.end();
}
