#include "clock.h"
#include <Arduino.h>
#include <time.h>

namespace {
	// Time servers: the NTP pool, and Google's as a fallback
	constexpr const char *PRIMARY_TIME_SERVER = "pool.ntp.org";
	constexpr const char *SECONDARY_TIME_SERVER = "time.google.com";

	// Any earlier time means the clock isn't set yet (it starts from 1970 at boot): 2026-01-01
	constexpr time_t EARLIEST_VALID_TIME = 1767225600;
}

// Set the clock from the internet (the ESP32 keeps asking in the background, and again every hour)
void setupClock() {
	configTime(0, 0, PRIMARY_TIME_SERVER, SECONDARY_TIME_SERVER); // UTC: the web app shows the times in the phone's own time zone
}

// Seconds since 1970, or 0 while the clock isn't set
uint32_t getUnixTime() {
	const time_t now = time(nullptr);
	return now >= EARLIEST_VALID_TIME ? static_cast<uint32_t>(now) : 0;
}
