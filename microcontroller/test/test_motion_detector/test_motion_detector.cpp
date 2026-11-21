#include <unity.h>
#include <string.h>

#include "detection/motion_detector.h"

namespace {
	// A frame of the size the firmware compares: a QVGA frame decoded at 1/8
	constexpr uint16_t WIDTH = 40;
	constexpr uint16_t HEIGHT = 30;
	constexpr size_t PIXELS = static_cast<size_t>(WIDTH) * HEIGHT;

	// Thresholds of the medium sensitivity: 6% of the picture changing by more than 30
	const MotionThresholds THRESHOLDS = { 30, 6 };

	MotionDetector detector;
	uint8_t frame[PIXELS];

	// Fill the frame with one brightness
	void fillFrame(uint8_t brightness) {
		memset(frame, brightness, sizeof(frame));
	}

	// Paint a square of another brightness (a bird at the entrance)
	void paintSquare(uint16_t left, uint16_t top, uint16_t size, uint8_t brightness) {
		for (uint16_t y = top; y < top + size; y++) {
			for (uint16_t x = left; x < left + size; x++) {
				frame[y * WIDTH + x] = brightness;
			}
		}
	}

	// Show the same still frame until the detector is past its warm-up
	void learnStillFrame(uint8_t brightness) {
		fillFrame(brightness);
		for (uint8_t index = 0; index < MOTION_WARMUP_FRAMES + 2; index++) {
			TEST_ASSERT_FALSE(detector.update(frame, WIDTH, HEIGHT, THRESHOLDS).isMotion);
		}
	}
}

// Start each test with a detector that has seen nothing
void setUp() {
	detector.reset();
}

// Nothing to clean up after each test
void tearDown() {}

// A still picture is never motion
void test_still_frames_are_not_motion() {
	learnStillFrame(100);
	const MotionResult result = detector.update(frame, WIDTH, HEIGHT, THRESHOLDS);
	TEST_ASSERT_FALSE(result.isMotion);
	TEST_ASSERT_EQUAL_UINT8(0, result.changedPercent);
}

// A bird covering part of the picture is motion
void test_a_bird_is_motion() {
	learnStillFrame(100);
	paintSquare(10, 10, 12, 20); // 144 of 1200 pixels: 12%
	const MotionResult result = detector.update(frame, WIDTH, HEIGHT, THRESHOLDS);
	TEST_ASSERT_TRUE(result.isMotion);
	TEST_ASSERT_EQUAL_UINT8(12, result.changedPercent);
}

// A small change (a moving leaf) stays under the threshold
void test_a_small_change_is_not_motion() {
	learnStillFrame(100);
	paintSquare(0, 0, 5, 220); // 25 of 1200 pixels: 2%
	TEST_ASSERT_FALSE(detector.update(frame, WIDTH, HEIGHT, THRESHOLDS).isMotion);
}

// A higher sensitivity sees the same small change
void test_a_higher_sensitivity_sees_small_changes() {
	learnStillFrame(100);
	paintSquare(0, 0, 7, 220); // 49 of 1200 pixels: 4%
	TEST_ASSERT_FALSE(detector.update(frame, WIDTH, HEIGHT, getMotionThresholds(Sensitivity::Medium)).isMotion);
	TEST_ASSERT_TRUE(detector.update(frame, WIDTH, HEIGHT, getMotionThresholds(Sensitivity::High)).isMotion);
}

// The whole picture getting brighter (a cloud passing, the sensor's exposure) is not motion
void test_a_change_of_light_is_not_motion() {
	learnStillFrame(100);
	fillFrame(160);
	TEST_ASSERT_FALSE(detector.update(frame, WIDTH, HEIGHT, THRESHOLDS).isMotion);
}

// Nothing is reported while the background is being learned, even if the frames change
void test_nothing_is_reported_while_warming_up() {
	for (uint8_t index = 0; index < MOTION_WARMUP_FRAMES; index++) {
		fillFrame(index % 2 == 0 ? 0 : 255);
		paintSquare(0, 0, 20, index % 2 == 0 ? 255 : 0);
		TEST_ASSERT_FALSE(detector.update(frame, WIDTH, HEIGHT, THRESHOLDS).isMotion);
	}
}

// A bird that stays blends into the background, so it stops being motion
void test_a_bird_that_stays_blends_in() {
	learnStillFrame(100);
	paintSquare(10, 10, 12, 20);
	TEST_ASSERT_TRUE(detector.update(frame, WIDTH, HEIGHT, THRESHOLDS).isMotion);
	bool isStillMotion = true;
	for (uint8_t index = 0; index < 60 && isStillMotion; index++) {
		isStillMotion = detector.update(frame, WIDTH, HEIGHT, THRESHOLDS).isMotion;
	}
	TEST_ASSERT_FALSE(isStillMotion);
}

// A frame of another size starts the background over, instead of comparing different pictures
void test_a_new_frame_size_starts_over() {
	learnStillFrame(100);
	uint8_t smallFrame[20 * 15];
	memset(smallFrame, 0, sizeof(smallFrame));
	TEST_ASSERT_FALSE(detector.update(smallFrame, 20, 15, THRESHOLDS).isMotion);
	TEST_ASSERT_FALSE(detector.update(smallFrame, 20, 15, THRESHOLDS).isMotion);
}

// Frames bigger than the background, empty or missing are refused
void test_refuses_frames_that_dont_fit() {
	TEST_ASSERT_FALSE(detector.update(frame, MOTION_FRAME_MAX_WIDTH + 1, 1, THRESHOLDS).isMotion);
	TEST_ASSERT_FALSE(detector.update(frame, 0, HEIGHT, THRESHOLDS).isMotion);
	TEST_ASSERT_FALSE(detector.update(nullptr, WIDTH, HEIGHT, THRESHOLDS).isMotion);
}

// Grayscale weighs green twice, and keeps black and white as they are
void test_converts_rgb_to_gray() {
	const uint8_t rgb[] = { 0, 0, 0, 255, 255, 255, 0, 255, 0, 255, 0, 0 };
	uint8_t gray[4];
	convertRgbToGray(rgb, 4, gray);
	TEST_ASSERT_EQUAL_UINT8(0, gray[0]);
	TEST_ASSERT_EQUAL_UINT8(255, gray[1]);
	TEST_ASSERT_EQUAL_UINT8(128, gray[2]);
	TEST_ASSERT_EQUAL_UINT8(64, gray[3]);
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_still_frames_are_not_motion);
	RUN_TEST(test_a_bird_is_motion);
	RUN_TEST(test_a_small_change_is_not_motion);
	RUN_TEST(test_a_higher_sensitivity_sees_small_changes);
	RUN_TEST(test_a_change_of_light_is_not_motion);
	RUN_TEST(test_nothing_is_reported_while_warming_up);
	RUN_TEST(test_a_bird_that_stays_blends_in);
	RUN_TEST(test_a_new_frame_size_starts_over);
	RUN_TEST(test_refuses_frames_that_dont_fit);
	RUN_TEST(test_converts_rgb_to_gray);
	return UNITY_END();
}
