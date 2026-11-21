#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

// Set the clock from the internet (NTP) once the Wi-Fi connects, so each visit has its date and time (call after the Wi-Fi setup)
void setupClock();

// Seconds since 1970 (UTC), or 0 while the clock isn't set (no internet since the last boot)
uint32_t getUnixTime();

#endif
