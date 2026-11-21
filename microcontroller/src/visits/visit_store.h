#ifndef VISIT_STORE_H
#define VISIT_STORE_H

#include <stddef.h>
#include <stdint.h>
#include "visit_index.h"
#include "visit_files.h"

// The microSD card and what the birdhouse stores on it
struct StorageStatus {
	bool isAvailable; // False without a card, or with a card that couldn't be read
	uint64_t totalBytes;
	uint64_t usedBytes;
	size_t visitCount;
};

// Why a visit couldn't be saved or removed
enum class VisitStoreResult : uint8_t {
	Ok,
	Unavailable, // No card
	NotFound, // No visit with this number
	Busy, // The visits deleted all at once are still being removed
	Failed // The card couldn't be written
};

// Mount the microSD card (1-bit mode, so the flash LED stays free) and read the visits on it (the firmware keeps running without a card)
void setupVisitStore();

// The card and its visits
StorageStatus getStorageStatus();

// Whether there is a card to save the visits on (quicker than getStorageStatus)
bool isStorageAvailable();

// Save a visit: its photo, and the small picture it was seen in (optional), making room first by removing the oldest visits
VisitStoreResult saveVisit(const uint8_t *photo, size_t photoSize, const uint8_t *thumbnail, size_t thumbnailSize, uint32_t timestamp, VisitEntry &entry);

// Copy a page of visits, newest first: count is set to how many were copied, total to how many there are
void copyVisitPage(size_t offset, size_t limit, VisitEntry *entries, size_t &count, size_t &total);

// Find a visit and the path of its photo, or of its small picture (false when there is none)
bool findVisitPath(uint32_t id, bool isThumbnail, char *path, size_t pathSize);

// Remove a visit, or every visit (quick: the files of every visit are removed later, by purgeDeletedVisits)
VisitStoreResult deleteVisit(uint32_t id);
VisitStoreResult deleteAllVisits();

// Remove the files of the visits deleted all at once, if there are any (slow on a full card: call it from a task that can wait)
void purgeDeletedVisits();

#endif
