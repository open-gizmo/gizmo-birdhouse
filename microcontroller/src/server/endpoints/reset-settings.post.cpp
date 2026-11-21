#include "../server.h"
#include <ArduinoJson.h>
#include <gizmo/device.h>
#include <gizmo/wifi_setup.h>
#include "../../settings/settings_storage.h"

// Clear the saved settings and the Wi-Fi, then restart: the factory reset of the birdhouse, which has no BOOT button to hold (on the ESP32-CAM, GPIO0 is the camera clock). The visits on the card stay
void setupResetSettingsPostRoute() {
	// POST to clear the settings (a GET could be triggered by any page on the network, for example with an image tag)
	server.on("/api/reset-settings", HTTP_POST, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Clear the settings and the saved network
		const bool isSettingsCleared = clearSettings();
		const bool isWifiCleared = gizmo::clearWifiCredentials();
		if (!isSettingsCleared || !isWifiCleared) {
			sendErrorResponse(request, 500, "clear_settings_failed", "Failed to clear the settings");
			return;
		}

		// Send the response, then restart from the main loop (so the response can leave before the reboot)
		sendSuccessResponse(request);
		gizmo::scheduleRestart();
	});

	// OPTIONS request for preflighted requests
	server.on("/api/reset-settings", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
		sendCorsPreflightResponse(request, "POST, OPTIONS");
	});
}
