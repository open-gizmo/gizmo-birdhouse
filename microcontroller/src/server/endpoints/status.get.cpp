#include "../server.h"
#include <ArduinoJson.h>
#include "../../clock/clock.h"
#include "../../detection/detection.h"
#include "../../settings/settings_storage.h"
#include "../../visits/visit_store.h"

namespace {
	// Build the JSON response with what the birdhouse is doing
	JsonDocument buildStatusData() {
		JsonDocument doc;
		JsonObject data = doc.to<JsonObject>();

		// Add the visit detection: switched on, watching, and how much of the picture changed last
		const DetectionStatus detectionStatus = getDetectionStatus();
		JsonObject detection = data["detection"].to<JsonObject>();
		detection["enabled"] = getSettings().isDetectionEnabled;
		detection["active"] = detectionStatus.isActive;
		detection["motionPercent"] = detectionStatus.lastMotionPercent;

		// Add the microSD card
		const StorageStatus storageStatus = getStorageStatus();
		JsonObject storage = data["storage"].to<JsonObject>();
		storage["available"] = storageStatus.isAvailable;
		storage["totalBytes"] = storageStatus.totalBytes;
		storage["usedBytes"] = storageStatus.usedBytes;

		// Add the visits: how many, and the newest one
		data["visitCount"] = storageStatus.visitCount;
		VisitEntry newest;
		size_t count = 0;
		size_t total = 0;
		copyVisitPage(0, 1, &newest, count, total);
		if (count > 0) {
			writeVisitToJson(data["lastVisit"].to<JsonObject>(), newest);
		} else {
			data["lastVisit"] = nullptr;
		}

		// Add whether the clock is set (the visits have no date without it)
		data["clockSet"] = getUnixTime() != 0;

		// Return the built JSON document
		return doc;
	}
}

// Get what the birdhouse is doing: the detection, the card and the visits
void setupStatusGetRoute() {
	server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Send the status
		JsonDocument doc = buildStatusData();
		sendSuccessResponse(request, 200, &doc);
	});
}
