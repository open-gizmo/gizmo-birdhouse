#include "../server.h"
#include <ArduinoJson.h>
#include <SD_MMC.h>
#include "../../visits/visit_files.h"
#include "../../visits/visit_store.h"

namespace {
	// Visits per page: 20 unless the request asks for another number, up to 50
	constexpr uint32_t PAGE_SIZE_DEFAULT = 20;
	constexpr uint32_t PAGE_SIZE_MAX = 50;

	// Send the error of a visit that couldn't be removed
	void sendVisitStoreError(AsyncWebServerRequest *request, VisitStoreResult result) {
		switch (result) {
			// No card
			case VisitStoreResult::Unavailable:
				sendErrorResponse(request, 503, "storage_unavailable", "No microSD card");
				break;

			// No visit with this number
			case VisitStoreResult::NotFound:
				sendErrorResponse(request, 404, "visit_not_found", "Visit not found");
				break;

			// The visits deleted all at once are still being removed
			case VisitStoreResult::Busy:
				sendErrorResponse(request, 409, "visits_busy", "The deleted visits are still being removed");
				break;

			// The card refused
			default:
				sendErrorResponse(request, 500, "visit_delete_failed", "Failed to delete the visit");
				break;
		}
	}

	// Read the page of a visit list request (false after sending the error)
	bool readPage(AsyncWebServerRequest *request, uint32_t &offset, uint32_t &limit) {
		offset = 0;
		limit = PAGE_SIZE_DEFAULT;
		const char *offsetText = getQueryValue(request, "offset");
		const char *limitText = getQueryValue(request, "limit");
		if (offsetText != nullptr && !parseWholeNumber(offsetText, offset)) {
			sendErrorResponse(request, 400, "invalid_page", "offset must be a whole number");
			return false;
		}
		if (limitText != nullptr && (!parseWholeNumber(limitText, limit) || limit == 0 || limit > PAGE_SIZE_MAX)) {
			sendErrorResponse(request, 400, "invalid_page", "limit must be a whole number from 1 to 50");
			return false;
		}
		return true;
	}

	// List the visits, newest first (GET /api/visits?offset=0&limit=20)
	void handleVisitsGet(AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Read the page
		uint32_t offset = 0;
		uint32_t limit = 0;
		if (!readPage(request, offset, limit)) {
			return;
		}

		// Copy the visits of the page
		VisitEntry entries[PAGE_SIZE_MAX];
		size_t count = 0;
		size_t total = 0;
		copyVisitPage(offset, limit, entries, count, total);

		// Send them with the total, so the web app knows when there are more
		JsonDocument doc;
		JsonObject data = doc.to<JsonObject>();
		data["total"] = total;
		data["offset"] = offset;
		JsonArray visits = data["visits"].to<JsonArray>();
		for (size_t index = 0; index < count; index++) {
			writeVisitToJson(visits.add<JsonObject>(), entries[index]);
		}
		sendSuccessResponse(request, 200, &doc);
	}

	// Send the photo of a visit, or its small picture (GET /api/visits/photo?id=12&thumbnail=1)
	void handlePhotoGet(AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Read the visit number
		uint32_t id = 0;
		if (!parseVisitId(getQueryValue(request, "id"), id)) {
			sendErrorResponse(request, 400, "invalid_visit_id", "id must be a visit number");
			return;
		}

		// Find its file
		const char *thumbnailText = getQueryValue(request, "thumbnail");
		const bool isThumbnail = thumbnailText != nullptr && strcmp(thumbnailText, "1") == 0;
		char path[VISIT_PATH_SIZE];
		if (!isStorageAvailable()) {
			sendErrorResponse(request, 503, "storage_unavailable", "No microSD card");
			return;
		}
		if (!findVisitPath(id, isThumbnail, path, sizeof(path)) || !SD_MMC.exists(path)) {
			sendErrorResponse(request, 404, "visit_not_found", "Visit not found");
			return;
		}

		// Send it from the card (a number is never given twice, so the browser can keep the file)
		AsyncWebServerResponse *response = request->beginResponse(SD_MMC, path, "image/jpeg");
		response->addHeader("Cache-Control", "private, max-age=31536000, immutable");
		request->send(response);
	}

	// Remove a visit (POST /api/visits/delete with { "id": 12 })
	void handleDeleteBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
		// Read the JSON body
		JsonDocument doc;
		if (!gizmo::readJsonBody(request, data, len, index, total, doc)) {
			return;
		}

		// Read the visit number
		JsonVariantConst idValue = doc["id"];
		if (!idValue.is<uint32_t>() || idValue.as<uint32_t>() == 0) {
			sendErrorResponse(request, 400, "invalid_visit_id", "id must be a visit number");
			return;
		}

		// Remove it
		const VisitStoreResult result = deleteVisit(idValue.as<uint32_t>());
		if (result != VisitStoreResult::Ok) {
			sendVisitStoreError(request, result);
			return;
		}
		sendSuccessResponse(request);
	}

	// Remove every visit (POST /api/visits/delete-all: a GET could be sent by any page on the network, with an image tag)
	void handleDeleteAll(AsyncWebServerRequest *request) {
		// Validate CORS
		if (!ensureCorsAllowed(request)) {
			return;
		}

		// Remove them
		const VisitStoreResult result = deleteAllVisits();
		if (result != VisitStoreResult::Ok) {
			sendVisitStoreError(request, result);
			return;
		}
		sendSuccessResponse(request);
	}
}

// Write a visit as the web API sends it
void writeVisitToJson(JsonObject json, const VisitEntry &entry) {
	json["id"] = entry.id;
	if (entry.timestamp != 0) {
		json["timestamp"] = entry.timestamp;
	} else {
		json["timestamp"] = nullptr;
	}
	json["sizeBytes"] = entry.sizeBytes;
}

// Setup the routes of the visits (the longer paths first: a route also answers the paths under it)
void setupVisitsRoutes() {
	// Photo of a visit
	server.on("/api/visits/photo", HTTP_GET, handlePhotoGet);

	// Remove every visit
	server.on("/api/visits/delete-all", HTTP_POST, handleDeleteAll);
	server.on("/api/visits/delete-all", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
		sendCorsPreflightResponse(request, "POST, OPTIONS");
	});

	// Remove a visit
	server.on("/api/visits/delete", HTTP_POST, [](AsyncWebServerRequest *request) {}, nullptr, handleDeleteBody);
	server.on("/api/visits/delete", HTTP_OPTIONS, [](AsyncWebServerRequest *request) {
		sendCorsPreflightResponse(request, "POST, OPTIONS");
	});

	// List of the visits
	server.on("/api/visits", HTTP_GET, handleVisitsGet);
}
