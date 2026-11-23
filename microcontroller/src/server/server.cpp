#include "server.h"
#include <gizmo/device.h>
#include "../config/config.h"

// Read a query parameter, or nullptr when the request leaves it out
const char *getQueryValue(AsyncWebServerRequest *request, const char *name) {
	const AsyncWebParameter *parameter = request->getParam(name);
	return parameter != nullptr ? parameter->value().c_str() : nullptr;
}

// Setup routes
void setupRoutes() {
	// Setup the routes (the device details, restart and forget network routes come from gizmo-common)
	gizmo::setupDeviceApi(server, { PROJECT_NAME, PIO_FIRMWARE_VERSION });
	setupCameraGetRoute();
	setupCaptureGetRoute();
	setupStatusGetRoute();
	setupVisitsRoutes();
	setupSettingsRoutes();
	setupResetSettingsPostRoute();
	setupAgentGuideRoute();
	setupCaptivePortalRoute();
}

// Setup server
void setupServer() {
	setupRoutes(); // Setup routes
	server.begin(); // Start the server
}
