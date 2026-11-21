#include <unity.h>

#include "visits/visit_index.h"

namespace {
	// The list under test (static: it holds every visit the birdhouse can keep)
	VisitIndex visitIndex;

	// A visit with a number, at a time made from it
	VisitEntry makeVisit(uint32_t id) {
		return { id, 1763721600 + id, 1000 + id };
	}
}

// Start each test with an empty list
void setUp() {
	visitIndex.clear();
}

// Nothing to clean up after each test
void tearDown() {}

// The newest visit comes first, whatever order the card listed them in
void test_lists_the_newest_first() {
	const uint32_t ids[] = { 5, 1, 9, 3 };
	for (uint32_t id : ids) {
		TEST_ASSERT_TRUE(visitIndex.add(makeVisit(id)));
	}
	TEST_ASSERT_EQUAL_UINT(4, visitIndex.count());
	TEST_ASSERT_EQUAL_UINT32(9, visitIndex.getNewest(0).id);
	TEST_ASSERT_EQUAL_UINT32(5, visitIndex.getNewest(1).id);
	TEST_ASSERT_EQUAL_UINT32(3, visitIndex.getNewest(2).id);
	TEST_ASSERT_EQUAL_UINT32(1, visitIndex.getNewest(3).id);
	TEST_ASSERT_EQUAL_UINT32(1, visitIndex.getOldest().id);
}

// A number is in the list once
void test_refuses_duplicate_numbers() {
	TEST_ASSERT_TRUE(visitIndex.add(makeVisit(4)));
	TEST_ASSERT_FALSE(visitIndex.add(makeVisit(4)));
	TEST_ASSERT_EQUAL_UINT(1, visitIndex.count());
}

// A visit is found by its number, with its details
void test_finds_visits() {
	visitIndex.add(makeVisit(2));
	visitIndex.add(makeVisit(7));
	VisitEntry entry = { 0, 0, 0 };
	TEST_ASSERT_TRUE(visitIndex.find(7, entry));
	TEST_ASSERT_EQUAL_UINT32(7, entry.id);
	TEST_ASSERT_EQUAL_UINT32(1763721607, entry.timestamp);
	TEST_ASSERT_EQUAL_UINT32(1007, entry.sizeBytes);
	TEST_ASSERT_FALSE(visitIndex.find(3, entry));
	TEST_ASSERT_FALSE(visitIndex.find(8, entry));
}

// Removing a visit keeps the others in order
void test_removes_visits() {
	for (uint32_t id = 1; id <= 5; id++) {
		visitIndex.add(makeVisit(id));
	}
	TEST_ASSERT_TRUE(visitIndex.remove(3));
	TEST_ASSERT_FALSE(visitIndex.remove(3));
	TEST_ASSERT_EQUAL_UINT(4, visitIndex.count());
	TEST_ASSERT_EQUAL_UINT32(5, visitIndex.getNewest(0).id);
	TEST_ASSERT_EQUAL_UINT32(4, visitIndex.getNewest(1).id);
	TEST_ASSERT_EQUAL_UINT32(2, visitIndex.getNewest(2).id);
	TEST_ASSERT_EQUAL_UINT32(1, visitIndex.getNewest(3).id);
}

// The list stops at its limit, so the store removes the oldest visit first
void test_stops_at_the_limit() {
	for (uint32_t id = 1; id <= MAX_VISITS; id++) {
		TEST_ASSERT_TRUE(visitIndex.add(makeVisit(id)));
	}
	TEST_ASSERT_TRUE(visitIndex.isFull());
	TEST_ASSERT_FALSE(visitIndex.add(makeVisit(MAX_VISITS + 1)));
	TEST_ASSERT_TRUE(visitIndex.remove(visitIndex.getOldest().id));
	TEST_ASSERT_TRUE(visitIndex.add(makeVisit(MAX_VISITS + 1)));
	TEST_ASSERT_EQUAL_UINT32(2, visitIndex.getOldest().id);
	TEST_ASSERT_EQUAL_UINT32(MAX_VISITS + 1, visitIndex.getNewest(0).id);
}

// A number is never given twice: not after the newest visit is deleted, and not after the card is emptied
void test_never_gives_a_number_twice() {
	TEST_ASSERT_EQUAL_UINT32(1, visitIndex.getNextId(0));
	visitIndex.add(makeVisit(1));
	visitIndex.add(makeVisit(2));
	TEST_ASSERT_EQUAL_UINT32(3, visitIndex.getNextId(2));
	visitIndex.remove(2);
	TEST_ASSERT_EQUAL_UINT32(3, visitIndex.getNextId(2));
	visitIndex.clear();
	TEST_ASSERT_EQUAL_UINT32(3, visitIndex.getNextId(2));

	// A card from another birdhouse, with higher numbers, goes on from its newest visit
	visitIndex.add(makeVisit(50));
	TEST_ASSERT_EQUAL_UINT32(51, visitIndex.getNextId(2));
}

// Run the tests
int main() {
	UNITY_BEGIN();
	RUN_TEST(test_lists_the_newest_first);
	RUN_TEST(test_refuses_duplicate_numbers);
	RUN_TEST(test_finds_visits);
	RUN_TEST(test_removes_visits);
	RUN_TEST(test_stops_at_the_limit);
	RUN_TEST(test_never_gives_a_number_twice);
	return UNITY_END();
}
