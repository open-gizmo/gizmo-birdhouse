#include "agent_guide.h"
#include <string.h>
#include <strings.h>

namespace {
	// Placeholder the device address replaces
	constexpr const char *HOST_PLACEHOLDER = "{host}";

	// The guide, with the address as a placeholder (kept in flash)
	constexpr const char *AGENT_GUIDE = R"(# Gizmo Birdhouse

A birdhouse camera on the local network: an ESP32-CAM with an OV2640 sensor and a white flash LED, inside a birdhouse. It takes a photo when a bird comes in and saves it on its microSD card, with the date and time. Each photo is a visit, numbered from 1 and never numbered twice.

## HTTP API

The birdhouse answers plain HTTP requests on the local network:

- `GET http://{host}/api/status` returns what the birdhouse is doing as JSON: the visit detection, the microSD card, the number of visits and the last one.
- `GET http://{host}/api/visits` returns the visits as JSON, newest first, with their `total`. The query takes `offset` and `limit` (20 by default, up to 50).
- `GET http://{host}/api/visits/photo?id=12` returns the JPEG of a visit; add `&thumbnail=1` for the small picture of the moment the bird was seen.
- `GET http://{host}/api/capture` takes a new photo right now and returns it as a JPEG. The query takes `resolution` (`QQVGA`, `QVGA`, `CIF`, `VGA`, `SVGA`, `XGA`, `HD`, `SXGA`, `UXGA`), `quality` (4 to 63, lower is sharper) and `flash` (`1` or `0`).
- `GET http://{host}/api/settings` returns the settings of the visit detection as JSON.
- `GET http://{host}/api/camera` returns the camera's details as JSON.

```sh
curl "http://{host}/api/visits?limit=5"
curl -o visit.jpg "http://{host}/api/visits/photo?id=12"
```

## Tips

- To see who came by, read `/api/visits` and fetch the photos you need: the small picture (`thumbnail=1`) is enough to tell whether there is a bird, and costs far fewer tokens.
- A visit's `timestamp` is in seconds since 1970, or `null` when the birdhouse didn't know the time yet.
- A photo of a visit never changes, so it can be kept; a visit can be deleted by its owner, and then answers `visit_not_found`.
- `/api/capture` shows the inside of the birdhouse right now. Its light is dim: turn the flash on only when the photo comes back dark, since it can scare a bird that is nesting.
- The camera takes one photo at a time. A busy camera answers an error (`camera_busy`), for example while it photographs a visit: wait a moment and try again.
- `storage_unavailable` means there is no microSD card: the visits can't be read, but `/api/capture` still works.
- Errors of the API are JSON: `{ "errorId": "visit_not_found", "errorMessage": "...", "data": null }`.
- The birdhouse only answers on its local network, and without authentication: anyone on that network can see its photos.
)";

	// Find the position of a media type in an Accept header, ignoring the case (-1 if it isn't there)
	int findMediaType(const char *accept, const char *mediaType) {
		const size_t acceptLength = strlen(accept);
		const size_t mediaTypeLength = strlen(mediaType);
		for (size_t position = 0; position + mediaTypeLength <= acceptLength; position++) {
			if (strncasecmp(accept + position, mediaType, mediaTypeLength) == 0) {
				return static_cast<int>(position);
			}
		}
		return -1;
	}
}

// Write the guide piece by piece, with the address the agent used
void writeAgentGuide(const char *host, AgentGuideWriter write, void *context) {
	const size_t placeholderLength = strlen(HOST_PLACEHOLDER);
	const char *remaining = AGENT_GUIDE;

	// Write the text between placeholders, and the address in their place
	const char *placeholder = strstr(remaining, HOST_PLACEHOLDER);
	while (placeholder != nullptr) {
		write(remaining, static_cast<size_t>(placeholder - remaining), context);
		write(host, strlen(host), context);
		remaining = placeholder + placeholderLength;
		placeholder = strstr(remaining, HOST_PLACEHOLDER);
	}

	// Write the end of the guide
	write(remaining, strlen(remaining), context);
}

// Check whether a client prefers Markdown to HTML from its Accept header
bool isMarkdownPreferred(const char *accept) {
	// No header: a plain request, which gets the web app
	if (accept == nullptr) {
		return false;
	}

	// Markdown must be listed, and come before HTML when both are (the quality values are not weighed)
	const int markdownPosition = findMediaType(accept, "text/markdown");
	const int htmlPosition = findMediaType(accept, "text/html");
	return markdownPosition >= 0 && (htmlPosition < 0 || markdownPosition < htmlPosition);
}