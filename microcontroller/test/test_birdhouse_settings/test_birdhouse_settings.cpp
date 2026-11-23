#include <unity.h>
#include <ArduinoJson.h>

#include "camera/capture_options.h"
#include "config/birdhouse_settings.h"

namespace {
	// Index of UXGA, the largest resolution of the OV2640 (the sensor of the ESP32-CAM)
	constexpr size_t UXGA_INDEX = 8;

	// Settings changed by the tests
	BirdhouseSettings settings;

	// Apply a JSON body to the settings, as POST /api/settings does
	bool apply(const char *body, const char *&invalidField) {
		JsonDocument doc;
		TEST_ASSERT_TRUE(deserializeJson(doc, body) == DeserializationError::Ok);
		return applySettingsFromJson(doc.as<JsonObjectConst>(), UXGA_INDEX, settings, invalidField);
	}
}

// Start each test from the defaults
void setUp() {
	settings = getDefaultSettings();
}

// Nothing to clean up after each test
void tearDown() {}

// A new birdhouse watches with the medium sensitivity, waits 30 s between photos, takes SXGA photos and keeps the flash off
void test_defaults() {
	TEST_ASSERT_TRUE(settings.isDetectionEnabled);
	TEST_ASSERT_EQUAL(Sensitivity::Medium, settings.sensitivity);
	TEST_ASSERT_EQUAL_UINT16(30, settings.cooldownSeconds);
	TEST_ASSERT_EQUAL_STRING("SXGA", RESOLUTIONS[settings.photoResolutionIndex].name);
	TEST_ASSERT_FALSE(settings.useFlash);
}

// A higher sensitivity needs smaller changes
void test_sensitivities_are_ordered() {
	const MotionThresholds low = getMotionThresholds(Sensitivity::Low);
	const MotionThresholds medium = getMotionThresholds(Sensitivity::Medium);
	const MotionThresholds high = getMotionThresholds(Sensitivity::High);
	TEST_ASSERT_TRUE(low.pixelDelta > medium.pixelDelta && medium.pixelDelta > high.pixelDelta);
	TEST_ASSERT_TRUE(low.changedPercent > medium.changedPercent && medium.changedPercent > high.changedPercent);
	TEST_ASSERT_TRUE(high.changedPercent > 0);
}

// Each sensitivity has a name, and the name finds it back
void test_sensitivity_names() {
	const Sensitivity sensitivities[] = { Sensitivity::Low, Sensitivity::Medium, Sensitivity::High };
	for (Sensitivity sensitivity : sensitivities) {
		Sensitivity found = Sensitivity::Low;
		TEST_ASSERT_TRUE(findSensitivity(getSensitivityName(sensitivity), found));
		TEST_ASSERT_EQUAL(sensitivity, found);
	}
	Sensitivity found = Sensitivity::Low;
	TEST_ASSERT_FALSE(findSensitivity("Medium", found));
	TEST_ASSERT_FALSE(findSensitivity("", found));
}

// The settings are written with the names of the web API
void test_writes_the_settings() {
	JsonDocument doc;
	writeSettingsToJson(doc.to<JsonObject>(), settings);
	TEST_ASSERT_TRUE(doc["detectionEnabled"].as<bool>());
	TEST_ASSERT_EQUAL_STRING("medium", doc["sensitivity"].as<const char *>());
	TEST_ASSERT_EQUAL_INT(30, doc["cooldownSeconds"].as<int>());
	TEST_ASSERT_EQUAL_STRING("SXGA", doc["photoResolution"].as<const char *>());
	TEST_ASSERT_FALSE(doc["flash"].as<bool>());
}

// Every field of a request is applied, and missing fields keep their value
void test_applies_the_fields_present() {
	const char *invalidField = nullptr;
	TEST_ASSERT_TRUE(apply("{\"detectionEnabled\":false,\"sensitivity\":\"high\",\"cooldownSeconds\":120,\"photoResolution\":\"vga\",\"flash\":true}", invalidField));
	TEST_ASSERT_FALSE(settings.isDetectionEnabled);
	TEST_ASSERT_EQUAL(Sensitivity::High, settings.sensitivity);
	TEST_ASSERT_EQUAL_UINT16(120, settings.cooldownSeconds);
	TEST_ASSERT_EQUAL_STRING("VGA", RESOLUTIONS[settings.photoResolutionIndex].name);
	TEST_ASSERT_TRUE(settings.useFlash);

	// A request with one field changes that field only
	TEST_ASSERT_TRUE(apply("{\"cooldownSeconds\":5}", invalidField));
	TEST_ASSERT_EQUAL_UINT16(5, settings.cooldownSeconds);
	TEST_ASSERT_EQUAL(Sensitivity::High, settings.sensitivity);
}

// Each invalid field is named, and a refused request changes nothing (not even its valid fields)
void test_refuses_invalid_fields() {
	struct Case {
		const char *body;
		const char *field;
	};
	const Case cases[] = {
		{ "{\"detectionEnabled\":1}", "detectionEnabled" },
		{ "{\"sensitivity\":\"extreme\"}", "sensitivity" },
		{ "{\"sensitivity\":2}", "sensitivity" },
		{ "{\"cooldownSeconds\":4}", "cooldownSeconds" },
		{ "{\"cooldownSeconds\":3601}", "cooldownSeconds" },
		{ "{\"cooldownSeconds\":30.5}", "cooldownSeconds" },
		{ "{\"cooldownSeconds\":\"30\"}", "cooldownSeconds" },
		{ "{\"photoResolution\":\"4K\"}", "photoResolution" },
		{ "{\"photoResolution\":\"FHD\"}", "photoResolution" },
		{ "{\"flash\":\"yes\"}", "flash" },
		{ "{\"sensitivity\":\"low\",\"flash\":\"yes\"}", "flash" }
	};
	for (const Case &testCase : cases) {
		const char *invalidField = nullptr;
		TEST_ASSERT_FALSE_MESSAGE(apply(testCase.body, invalidField), testCase.body);
		TEST_ASSERT_EQUAL_STRING_MESSAGE(testCase.field, invalidField, testCase.body);
	}
	TEST_ASSERT_EQUAL(Sensitivity::Medium, settings.sensitivity);
	TEST_ASSERT_FALSE(settings.useFlash);
}

// The cooldown range ends are accepted
void test_accepts_the_cooldown_range_ends() {
	const char *invalidField = nullptr;
	TEST_ASSERT_TRUE(apply("{\"cooldownSeconds\":5}", invalidField));
	TEST_ASSERT_TRUE(apply("{\"cooldownSeconds\":3600}", invalidField));
	TEST_ASSERT_EQUAL_UINT16(3600, settings.cooldownSeconds);
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_defaults);
	RUN_TEST(test_sensitivities_are_ordered);
	RUN_TEST(test_sensitivity_names);
	RUN_TEST(test_writes_the_settings);
	RUN_TEST(test_applies_the_fields_present);
	RUN_TEST(test_refuses_invalid_fields);
	RUN_TEST(test_accepts_the_cooldown_range_ends);
	return UNITY_END();
}
