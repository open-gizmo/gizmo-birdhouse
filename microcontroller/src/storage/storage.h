#ifndef STORAGE_H
#define STORAGE_H

#include "../config/birdhouse_settings.h"

// Load the saved settings (the defaults on a new birdhouse), clamped to what the sensor can take
void setupSettings(size_t maxResolutionIndex);

// The current settings (a copy: safe to call from any task)
BirdhouseSettings getSettings();

// Save the settings and use them (false when the flash couldn't be written, and nothing changes)
bool saveSettings(const BirdhouseSettings &settings);

// Forget the saved settings (the defaults apply after the next boot; the visit numbers keep counting)
bool clearSettings();

// The number of the last visit taken, kept apart from the card so a number is never given twice
uint32_t getLastVisitId();
void saveLastVisitId(uint32_t id);

#endif
