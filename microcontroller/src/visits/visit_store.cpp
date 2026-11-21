#include "visit_store.h"
#include <Arduino.h>
#include <SD_MMC.h>
#include <atomic>
#include <freertos/semphr.h>
#include "../settings/settings_storage.h"
#include "../serial/serial.h"

namespace {
	// Room to keep free on the card: the oldest visits go when a new photo would leave less
	constexpr uint64_t MIN_FREE_BYTES = 4ULL * 1024 * 1024;

	// Mount point of the card in the file system of the ESP32
	constexpr const char *MOUNT_POINT = "/sdcard";

	// Visits on the card (in PSRAM: the list is 12 KB), and the lock that guards it and the writes to the card
	VisitIndex *visitIndex = nullptr;
	SemaphoreHandle_t storeLock = nullptr;
	bool isCardAvailable = false;
	std::atomic<bool> hasDeletedVisits(false); // Files of visits deleted all at once are still on the card

	// Hold the store for the time of a block
	class StoreLock {
	public:
		StoreLock() {
			xSemaphoreTake(storeLock, portMAX_DELAY);
		}
		~StoreLock() {
			xSemaphoreGive(storeLock);
		}
	};

	// Free bytes on the card
	uint64_t getFreeBytes() {
		const uint64_t totalBytes = SD_MMC.totalBytes();
		const uint64_t usedBytes = SD_MMC.usedBytes();
		return totalBytes > usedBytes ? totalBytes - usedBytes : 0;
	}

	// Remove the files of a visit (the small picture may be missing)
	bool removeVisitFiles(const VisitEntry &entry) {
		char path[VISIT_PATH_SIZE];
		bool isRemoved = formatVisitPath(path, sizeof(path), entry.id, entry.timestamp, false) && SD_MMC.remove(path);
		if (formatVisitPath(path, sizeof(path), entry.id, entry.timestamp, true) && SD_MMC.exists(path)) {
			SD_MMC.remove(path);
		}
		return isRemoved;
	}

	// Remove the oldest visit (held by the caller)
	bool removeOldestVisit() {
		if (visitIndex->count() == 0) {
			return false;
		}
		const VisitEntry oldest = visitIndex->getOldest();
		removeVisitFiles(oldest);
		visitIndex->remove(oldest.id);
		printLogfln("Removed visit %lu to make room", static_cast<unsigned long>(oldest.id));
		return true;
	}

	// Write a file, removing it when it couldn't be written whole
	bool writeFile(const char *path, const uint8_t *data, size_t size) {
		File file = SD_MMC.open(path, FILE_WRITE);
		if (!file) {
			return false;
		}
		const size_t written = file.write(data, size);
		file.close();
		if (written != size) {
			SD_MMC.remove(path);
			return false;
		}
		return true;
	}

	// Read the visits in the photos folder into the list
	void loadVisits() {
		File folder = SD_MMC.open(VISITS_FOLDER);
		if (!folder || !folder.isDirectory()) {
			return;
		}

		// Add every visit photo (the other files and folders are left alone)
		size_t skipped = 0;
		File file = folder.openNextFile();
		while (file) {
			uint32_t id = 0;
			uint32_t timestamp = 0;
			if (!file.isDirectory() && parseVisitFileName(file.name(), id, timestamp)) {
				if (!visitIndex->add({ id, timestamp, static_cast<uint32_t>(file.size()) })) {
					skipped++;
				}
			}
			file.close();
			file = folder.openNextFile();
		}
		folder.close();

		// Say how many were found (beyond the limit, the newest ones are left out of the list until older ones are removed)
		printLogfln("Visits on the card: %u", static_cast<unsigned>(visitIndex->count()));
		if (skipped > 0) {
			printLogfln("Visits left out of the list: %u", static_cast<unsigned>(skipped));
		}
	}
}

// Mount the microSD card and read the visits on it
void setupVisitStore() {
	storeLock = xSemaphoreCreateMutex();
	visitIndex = new VisitIndex();

	// Mount the card in 1-bit mode: the 4-bit mode also takes GPIO4, the flash LED, which would light up with every write
	if (!SD_MMC.begin(MOUNT_POINT, true)) {
		printLogln("No microSD card: the visits won't be saved");
		return;
	}
	if (SD_MMC.cardType() == CARD_NONE) {
		printLogln("No microSD card: the visits won't be saved");
		SD_MMC.end();
		return;
	}

	// Create the folders of the visits
	if (!SD_MMC.exists(VISITS_FOLDER) && !SD_MMC.mkdir(VISITS_FOLDER)) {
		printLogln("Failed to create the visits folder on the card");
		return;
	}
	if (!SD_MMC.exists(THUMBNAILS_FOLDER)) {
		SD_MMC.mkdir(THUMBNAILS_FOLDER);
	}

	// Read the visits already on the card (and finish removing the ones deleted before a restart)
	isCardAvailable = true;
	hasDeletedVisits = SD_MMC.exists(DELETED_VISITS_FOLDER);
	printLogfln("microSD card: %llu MB", SD_MMC.totalBytes() / (1024 * 1024));
	loadVisits();
}

// The card and its visits
StorageStatus getStorageStatus() {
	StoreLock lock;
	if (!isCardAvailable) {
		return { false, 0, 0, 0 };
	}
	return { true, SD_MMC.totalBytes(), SD_MMC.usedBytes(), visitIndex->count() };
}

// Whether there is a card to save the visits on (set once at boot)
bool isStorageAvailable() {
	return isCardAvailable;
}

// Save a visit
VisitStoreResult saveVisit(const uint8_t *photo, size_t photoSize, const uint8_t *thumbnail, size_t thumbnailSize, uint32_t timestamp, VisitEntry &entry) {
	StoreLock lock;
	if (!isCardAvailable) {
		return VisitStoreResult::Unavailable;
	}

	// Make room: the list has a limit, and the card must keep some space free
	while (visitIndex->isFull() || (getFreeBytes() < photoSize + thumbnailSize + MIN_FREE_BYTES && visitIndex->count() > 0)) {
		removeOldestVisit();
	}

	// Write the photo under the next number (kept in the flash, so a deleted number is never given again)
	const uint32_t id = visitIndex->getNextId(getLastVisitId());
	char path[VISIT_PATH_SIZE];
	if (!formatVisitPath(path, sizeof(path), id, timestamp, false) || !writeFile(path, photo, photoSize)) {
		printLogln("Failed to write the visit photo");
		return VisitStoreResult::Failed;
	}
	saveLastVisitId(id);

	// Write the small picture (without it, the web app shows the photo instead)
	if (thumbnail != nullptr && thumbnailSize > 0 && formatVisitPath(path, sizeof(path), id, timestamp, true)) {
		writeFile(path, thumbnail, thumbnailSize);
	}

	// Add the visit to the list
	entry = { id, timestamp, static_cast<uint32_t>(photoSize) };
	visitIndex->add(entry);
	return VisitStoreResult::Ok;
}

// Copy a page of visits, newest first
void copyVisitPage(size_t offset, size_t limit, VisitEntry *entries, size_t &count, size_t &total) {
	StoreLock lock;
	count = 0;
	total = isCardAvailable ? visitIndex->count() : 0;
	for (size_t position = offset; position < total && count < limit; position++) {
		entries[count++] = visitIndex->getNewest(position);
	}
}

// Find a visit and the path of one of its files
bool findVisitPath(uint32_t id, bool isThumbnail, char *path, size_t pathSize) {
	StoreLock lock;
	VisitEntry entry;
	if (!isCardAvailable || !visitIndex->find(id, entry)) {
		return false;
	}

	// The small picture, when it's on the card, or the photo
	if (isThumbnail && formatVisitPath(path, pathSize, entry.id, entry.timestamp, true) && SD_MMC.exists(path)) {
		return true;
	}
	return formatVisitPath(path, pathSize, entry.id, entry.timestamp, false);
}

// Remove a visit
VisitStoreResult deleteVisit(uint32_t id) {
	StoreLock lock;
	if (!isCardAvailable) {
		return VisitStoreResult::Unavailable;
	}
	VisitEntry entry;
	if (!visitIndex->find(id, entry)) {
		return VisitStoreResult::NotFound;
	}

	// Remove its files, then forget it
	if (!removeVisitFiles(entry)) {
		return VisitStoreResult::Failed;
	}
	visitIndex->remove(id);
	return VisitStoreResult::Ok;
}

// Remove every visit: the folder is renamed at once, and its files are removed later by purgeDeletedVisits
VisitStoreResult deleteAllVisits() {
	StoreLock lock;
	if (!isCardAvailable) {
		return VisitStoreResult::Unavailable;
	}

	// Wait for the files of an earlier removal to be gone
	if (hasDeletedVisits) {
		return VisitStoreResult::Busy;
	}

	// Move the folder away, and start a new empty one
	if (!SD_MMC.rename(VISITS_FOLDER, DELETED_VISITS_FOLDER)) {
		return VisitStoreResult::Failed;
	}
	SD_MMC.mkdir(VISITS_FOLDER);
	SD_MMC.mkdir(THUMBNAILS_FOLDER);
	visitIndex->clear();
	hasDeletedVisits = true;
	return VisitStoreResult::Ok;
}

// Remove the files of the visits deleted all at once (slow on a full card, so the detection task does it, file by file)
void purgeDeletedVisits() {
	if (!isCardAvailable || !hasDeletedVisits) {
		return;
	}

	// Remove the small pictures, then the photos, then the folders (the new visits are in another folder, so the store isn't held meanwhile)
	char filePath[300]; // Any name the card can hold (a file put there by hand too), so the folder can be removed
	const char *folders[] = { DELETED_THUMBNAILS_FOLDER, DELETED_VISITS_FOLDER };
	for (const char *folderName : folders) {
		File folder = SD_MMC.open(folderName);
		if (folder && folder.isDirectory()) {
			File file = folder.openNextFile();
			while (file) {
				const bool isFile = !file.isDirectory();
				snprintf(filePath, sizeof(filePath), "%s/%s", folderName, file.name());
				file.close();
				if (isFile) {
					SD_MMC.remove(filePath);
				}
				file = folder.openNextFile();
			}
			folder.close();
		}
		SD_MMC.rmdir(folderName);
	}
	hasDeletedVisits = SD_MMC.exists(DELETED_VISITS_FOLDER); // Tried again on the next call if a file stayed
	printLogln(hasDeletedVisits ? "Some deleted visits are still on the card" : "Deleted visits removed from the card");
}
