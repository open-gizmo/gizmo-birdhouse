#ifndef SERVER_H
#define SERVER_H

#include <ArduinoJson.h>
#include <gizmo/http.h>
#include "../config/config.h"
#include "../visits/visit_index.h"

// Standard API responses and CORS checks come from gizmo-common
using gizmo::ensureCorsAllowed;
using gizmo::sendCorsPreflightResponse;
using gizmo::sendErrorResponse;
using gizmo::sendSuccessResponse;

// Web server routes (the Wi-Fi setup and device routes are registered by gizmo-common)
void setupCameraGetRoute();
void setupCaptureGetRoute();
void setupStatusGetRoute();
void setupVisitsRoutes();
void setupSettingsRoutes();
void setupResetSettingsPostRoute();
void setupAgentGuideRoute();
void setupCaptivePortalRoute();

// Send the guide for AI agents (Markdown)
void sendAgentGuide(AsyncWebServerRequest *request);

// Read a query parameter, or nullptr when the request leaves it out (the pointer stays valid while the request lives)
const char *getQueryValue(AsyncWebServerRequest *request, const char *name);

// Write a visit as the web API sends it: { id, timestamp (null while the clock wasn't set), sizeBytes }
void writeVisitToJson(JsonObject json, const VisitEntry &entry);

// Server setup
void setupRoutes();
void setupServer();

#endif
