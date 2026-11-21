#include "visit_files.h"
#include <stdio.h>
#include <string.h>

namespace {
	// Extension of the visit files
	constexpr const char *PHOTO_EXTENSION = ".jpg";

	// Read a whole number of up to 10 digits that fits 32 bits, moving the text past it (false without digits, or when it's too big)
	bool readNumber(const char *&text, uint32_t &value) {
		uint64_t number = 0;
		size_t digits = 0;
		while (*text >= '0' && *text <= '9') {
			number = number * 10 + static_cast<uint64_t>(*text - '0');
			digits++;
			text++;
			if (digits > 10 || number > UINT32_MAX) {
				return false;
			}
		}
		if (digits == 0) {
			return false;
		}
		value = static_cast<uint32_t>(number);
		return true;
	}
}

// Write the file name of a visit
bool formatVisitFileName(char *buffer, size_t bufferSize, uint32_t id, uint32_t timestamp) {
	const int length = snprintf(buffer, bufferSize, "%lu_%lu%s", static_cast<unsigned long>(id), static_cast<unsigned long>(timestamp), PHOTO_EXTENSION);
	return length > 0 && static_cast<size_t>(length) < bufferSize;
}

// Read the number and the time of a visit from its file name
bool parseVisitFileName(const char *name, uint32_t &id, uint32_t &timestamp) {
	// Nothing to read without a name
	if (name == nullptr) {
		return false;
	}

	// Read the number, from 1
	const char *cursor = name;
	uint32_t parsedId = 0;
	if (!readNumber(cursor, parsedId) || parsedId == 0 || *cursor != '_') {
		return false;
	}
	cursor++;

	// Read the time, then the extension, which ends the name
	uint32_t parsedTimestamp = 0;
	if (!readNumber(cursor, parsedTimestamp) || strcmp(cursor, PHOTO_EXTENSION) != 0) {
		return false;
	}

	// Return them once the whole name is read
	id = parsedId;
	timestamp = parsedTimestamp;
	return true;
}

// Write the path of a visit photo, or of its small picture
bool formatVisitPath(char *buffer, size_t bufferSize, uint32_t id, uint32_t timestamp, bool isThumbnail) {
	char fileName[VISIT_FILE_NAME_SIZE];
	if (!formatVisitFileName(fileName, sizeof(fileName), id, timestamp)) {
		return false;
	}
	const int length = snprintf(buffer, bufferSize, "%s/%s", isThumbnail ? THUMBNAILS_FOLDER : VISITS_FOLDER, fileName);
	return length > 0 && static_cast<size_t>(length) < bufferSize;
}

// Read a whole number from the text of a request
bool parseWholeNumber(const char *text, uint32_t &value) {
	// Nothing to read without a text
	if (text == nullptr) {
		return false;
	}

	// Read the digits, which must be the whole text
	const char *cursor = text;
	uint32_t parsedValue = 0;
	if (!readNumber(cursor, parsedValue) || *cursor != '\0') {
		return false;
	}
	value = parsedValue;
	return true;
}

// Read a visit number from the text of a request
bool parseVisitId(const char *text, uint32_t &id) {
	uint32_t parsedId = 0;
	if (!parseWholeNumber(text, parsedId) || parsedId == 0) {
		return false;
	}
	id = parsedId;
	return true;
}
