#include "../server.h"
#include <ArduinoJson.h>
#include "../../camera/camera.h"
#include "../../storage/storage.h"
#include "../../serial/serial.h"

namespace {
	// Send the current settings
	void sendSettings(AsyncWebServerRequest *request) {
		JsonDocument doc;
		writeSettingsToJson(doc.to<JsonObject>(), getSettings());
		sendSuccessResponse(request, 200, &doc);
	}

	// Change the settings the body has (POST /api/settings with { "detectionEnabled": true, "sensitivity": "medium", ... }, every field optional)
	void handleSettingsBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		// Read the JSON body
		JsonDocument doc;
		if (!gizmo::readJsonBody(request, data, len, index, total, doc)) {
			return;
		}

		// Apply the fields to the current settings, refusing the request on the first invalid one
		BirdhouseSettings settings = getSettings();
		const char *invalidField = nullptr;
		if (!applySettingsFromJson(doc.as<JsonObjectConst>(), getCameraInfo().maxResolutionIndex, settings, invalidField)) {
			printLogfln("Invalid settings field: %s", invalidField);
			sendErrorResponse(request, 400, "invalid_settings_field", invalidField);
			return;
		}

		// Save them (the detection reads them on its next frame)
		if (!saveSettings(settings)) {
			sendErrorResponse(request, 500, "settings_save_failed", "Failed to save the settings");
			return;
		}
		sendSettings(request);
	}
}

// Setup the routes of the settings
void setupSettingsRoutes() {
	// Read the settings
	server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (!ensureCorsAllowed(request)) {
			return;
		}
		sendSettings(request);
	});

	// Change the settings
	server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, handleSettingsBody);

	// OPTIONS request for preflighted requests
	server.on("/api/settings", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
		sendCorsPreflightResponse(request, "GET, POST, OPTIONS");
	});
}
