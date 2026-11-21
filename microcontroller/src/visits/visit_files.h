#ifndef VISIT_FILES_H
#define VISIT_FILES_H

#include <stddef.h>
#include <stdint.h>

// Names of the visit files on the microSD card, kept free of hardware headers so the native tests can use them

// Folders of the visits: the photos, and a small picture of each (the frame the visit was seen in)
constexpr const char *VISITS_FOLDER = "/visits";
constexpr const char *THUMBNAILS_FOLDER = "/visits/thumbs";

// Where every visit goes when they are deleted all at once, until its files are removed
constexpr const char *DELETED_VISITS_FOLDER = "/visits-deleted";
constexpr const char *DELETED_THUMBNAILS_FOLDER = "/visits-deleted/thumbs";

// Longest path of a visit file ("/visits-deleted/thumbs/" + a name), with its end
constexpr size_t VISIT_PATH_SIZE = 56;

// Longest file name of a visit ("4294967295_4294967295.jpg"), with its end
constexpr size_t VISIT_FILE_NAME_SIZE = 26;

// Write the file name of a visit: its number and the time it was taken, in seconds since 1970 (0 when the clock wasn't set), for example "42_1763721600.jpg"
bool formatVisitFileName(char *buffer, size_t bufferSize, uint32_t id, uint32_t timestamp);

// Read the number and the time of a visit from its file name (false for any other file, so foreign files on the card are left alone)
bool parseVisitFileName(const char *name, uint32_t &id, uint32_t &timestamp);

// Write the path of a visit photo, or of its small picture
bool formatVisitPath(char *buffer, size_t bufferSize, uint32_t id, uint32_t timestamp, bool isThumbnail);

// Read a whole number from the text of a request: digits only, up to 4294967295 (false otherwise)
bool parseWholeNumber(const char *text, uint32_t &value);

// Read a visit number from the text of a request: a whole number from 1 (false otherwise)
bool parseVisitId(const char *text, uint32_t &id);

#endif
