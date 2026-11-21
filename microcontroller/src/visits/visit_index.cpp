#include "visit_index.h"
#include <string.h>

// Forget every visit
void VisitIndex::clear() {
	entryCount = 0;
}

// Position of the first visit whose number isn't smaller than id (a binary search: the numbers are sorted)
size_t VisitIndex::findPosition(uint32_t id) const {
	size_t low = 0;
	size_t high = entryCount;
	while (low < high) {
		const size_t middle = low + (high - low) / 2;
		if (entries[middle].id < id) {
			low = middle + 1;
		} else {
			high = middle;
		}
	}
	return low;
}

// Add a visit in its place
bool VisitIndex::add(const VisitEntry &entry) {
	// Refuse it when full, or when the number is already there
	if (entryCount >= MAX_VISITS) {
		return false;
	}
	const size_t position = findPosition(entry.id);
	if (position < entryCount && entries[position].id == entry.id) {
		return false;
	}

	// Make room and insert it (a new visit goes last, so nothing moves)
	memmove(&entries[position + 1], &entries[position], (entryCount - position) * sizeof(VisitEntry));
	entries[position] = entry;
	entryCount++;
	return true;
}

// Remove a visit
bool VisitIndex::remove(uint32_t id) {
	const size_t position = findPosition(id);
	if (position >= entryCount || entries[position].id != id) {
		return false;
	}
	memmove(&entries[position], &entries[position + 1], (entryCount - position - 1) * sizeof(VisitEntry));
	entryCount--;
	return true;
}

// Find a visit by number
bool VisitIndex::find(uint32_t id, VisitEntry &entry) const {
	const size_t position = findPosition(id);
	if (position >= entryCount || entries[position].id != id) {
		return false;
	}
	entry = entries[position];
	return true;
}

// Number of visits
size_t VisitIndex::count() const {
	return entryCount;
}

// Whether no more visits fit
bool VisitIndex::isFull() const {
	return entryCount >= MAX_VISITS;
}

// The visit at a position, counted from the newest one
const VisitEntry &VisitIndex::getNewest(size_t position) const {
	return entries[entryCount - 1 - position];
}

// The oldest visit
const VisitEntry &VisitIndex::getOldest() const {
	return entries[0];
}

// The number after the newest visit, or after a number already used
uint32_t VisitIndex::getNextId(uint32_t lastUsedId) const {
	const uint32_t newestId = entryCount > 0 ? entries[entryCount - 1].id : 0;
	const uint32_t lastId = newestId > lastUsedId ? newestId : lastUsedId;
	return lastId + 1;
}
