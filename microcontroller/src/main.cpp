#include "serial/serial.h"
#include "camera/camera.h"
#include "storage/storage.h"
#include "visits/visit_store.h"
#include "wifi/wifi.h"
#include "clock/clock.h"
#include "detection/detection.h"
#include "server/server.h"
#include <gizmo/device.h>

// Setup
void setup() {
	initSerial(); // Init serial
	setupCamera(); // Start the camera first, while the PSRAM is free for its frame buffers
	setupSettings(getCameraInfo().maxResolutionIndex); // Load the settings, for a sensor that can take their resolution
	setupVisitStore(); // Mount the microSD card and read the visits on it
	setupWiFi(); // Start the Wi-Fi setup and its web API
	setupClock(); // Set the clock from the internet once the Wi-Fi connects, so the visits have a date
	setupServer(); // Setup server
	setupDetection(); // Watch for birds

	// No factory reset with the BOOT button: on the ESP32-CAM, GPIO0 is the clock of the camera (the web app resets the settings, and a missing network opens the setup hotspot again)
	manageWiFiConnection(); // Manage the Wi-Fi connection
}

// Loop
void loop() {
	// Restart the device if the web server asked for it
	gizmo::manageDevice();

	// Manage the Wi-Fi connection
	manageWiFiConnection();

	// Give the CPU back between checks (the ESP32-CAM runs warm)
	delay(10);
}
