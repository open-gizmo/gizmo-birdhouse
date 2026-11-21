#include <unity.h>

#include "visits/visit_files.h"

// Nothing to set up before each test
void setUp() {}

// Nothing to clean up after each test
void tearDown() {}

// A visit file is named by its number and its time
void test_formats_visit_file_names() {
	char name[VISIT_FILE_NAME_SIZE];
	TEST_ASSERT_TRUE(formatVisitFileName(name, sizeof(name), 42, 1763721600));
	TEST_ASSERT_EQUAL_STRING("42_1763721600.jpg", name);
	TEST_ASSERT_TRUE(formatVisitFileName(name, sizeof(name), 4294967295UL, 4294967295UL));
	TEST_ASSERT_EQUAL_STRING("4294967295_4294967295.jpg", name);
}

// A buffer too small for the name is refused, never cut
void test_refuses_small_buffers() {
	char name[10];
	TEST_ASSERT_FALSE(formatVisitFileName(name, sizeof(name), 42, 1763721600));
}

// The paths of a photo and of its small picture
void test_formats_visit_paths() {
	char path[VISIT_PATH_SIZE];
	TEST_ASSERT_TRUE(formatVisitPath(path, sizeof(path), 7, 0, false));
	TEST_ASSERT_EQUAL_STRING("/visits/7_0.jpg", path);
	TEST_ASSERT_TRUE(formatVisitPath(path, sizeof(path), 7, 1763721600, true));
	TEST_ASSERT_EQUAL_STRING("/visits/thumbs/7_1763721600.jpg", path);
}

// The longest name fits the path buffer, in the folder of the deleted visits too
void test_longest_path_fits() {
	TEST_ASSERT_TRUE(sizeof("/visits-deleted/thumbs/") - 1 + VISIT_FILE_NAME_SIZE <= VISIT_PATH_SIZE);
}

// A name written by the firmware reads back the same
void test_reads_visit_file_names() {
	uint32_t id = 0;
	uint32_t timestamp = 0;
	TEST_ASSERT_TRUE(parseVisitFileName("42_1763721600.jpg", id, timestamp));
	TEST_ASSERT_EQUAL_UINT32(42, id);
	TEST_ASSERT_EQUAL_UINT32(1763721600, timestamp);
	TEST_ASSERT_TRUE(parseVisitFileName("1_0.jpg", id, timestamp));
	TEST_ASSERT_EQUAL_UINT32(1, id);
	TEST_ASSERT_EQUAL_UINT32(0, timestamp);
}

// Other files on the card are left alone: they are never read as visits
void test_refuses_other_files() {
	uint32_t id = 99;
	uint32_t timestamp = 99;
	const char *names[] = { "", "photo.jpg", "42.jpg", "42_.jpg", "_42.jpg", "0_1763721600.jpg", "42_1763721600.JPG", "42_1763721600.jpg.bak", "42_1763721600", "-1_5.jpg", "42_17637x1600.jpg", "4294967296_1.jpg", "42_99999999999.jpg", "thumbs" };
	for (const char *name : names) {
		TEST_ASSERT_FALSE_MESSAGE(parseVisitFileName(name, id, timestamp), name);
	}
	TEST_ASSERT_FALSE(parseVisitFileName(nullptr, id, timestamp));
	TEST_ASSERT_EQUAL_UINT32(99, id);
	TEST_ASSERT_EQUAL_UINT32(99, timestamp);
}

// The numbers of a request: digits only, the visit numbers from 1
void test_reads_request_numbers() {
	uint32_t value = 99;
	TEST_ASSERT_TRUE(parseWholeNumber("0", value));
	TEST_ASSERT_EQUAL_UINT32(0, value);
	TEST_ASSERT_TRUE(parseWholeNumber("4294967295", value));
	TEST_ASSERT_EQUAL_UINT32(4294967295UL, value);
	TEST_ASSERT_TRUE(parseVisitId("12", value));
	TEST_ASSERT_EQUAL_UINT32(12, value);
	const char *invalid[] = { "", " 1", "1 ", "+1", "-1", "1.5", "0x10", "4294967296", "abc" };
	for (const char *text : invalid) {
		TEST_ASSERT_FALSE_MESSAGE(parseWholeNumber(text, value), text);
	}
	TEST_ASSERT_FALSE(parseVisitId("0", value));
	TEST_ASSERT_FALSE(parseVisitId(nullptr, value));
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_formats_visit_file_names);
	RUN_TEST(test_refuses_small_buffers);
	RUN_TEST(test_formats_visit_paths);
	RUN_TEST(test_longest_path_fits);
	RUN_TEST(test_reads_visit_file_names);
	RUN_TEST(test_refuses_other_files);
	RUN_TEST(test_reads_request_numbers);
	return UNITY_END();
}
