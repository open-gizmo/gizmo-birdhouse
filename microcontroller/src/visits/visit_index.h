#ifndef VISIT_INDEX_H
#define VISIT_INDEX_H

#include <stddef.h>
#include <stdint.h>

// The list of the visits on the card, kept in memory and free of hardware headers so the native tests can use it

// Most visits the birdhouse keeps: the oldest one makes room for a new one (about 150 MB of photos at the default resolution)
constexpr size_t MAX_VISITS = 1000;

// One visit: its number, when it was taken and the size of its photo
struct VisitEntry {
	uint32_t id; // From 1, never reused
	uint32_t timestamp; // Seconds since 1970, 0 when the clock wasn't set
	uint32_t sizeBytes;
};

// Visits sorted by number, oldest first
class VisitIndex {
public:
	// Forget every visit
	void clear();

	// Add a visit in its place (false when it's full, or the number is already there)
	bool add(const VisitEntry &entry);

	// Remove a visit (false when there is none with this number)
	bool remove(uint32_t id);

	// Find a visit by number (false when there is none)
	bool find(uint32_t id, VisitEntry &entry) const;

	// Number of visits
	size_t count() const;

	// Whether no more visits fit
	bool isFull() const;

	// The visit at a position, counted from the newest one (0 is the newest): call with a position under count()
	const VisitEntry &getNewest(size_t position) const;

	// The oldest visit: call when count() is not 0
	const VisitEntry &getOldest() const;

	// The number after the newest visit, or after a number already used (so a deleted number is never given again)
	uint32_t getNextId(uint32_t lastUsedId) const;

private:
	VisitEntry entries[MAX_VISITS] = {};
	size_t entryCount = 0;

	// Position of the first visit whose number isn't smaller than id
	size_t findPosition(uint32_t id) const;
};

#endif
