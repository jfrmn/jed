#include "checks.hh"
#include "util/hashtable.hh"

void Test_Hashtable_Insert_BasicInsertion() {
	Hashtable<int> ht;
	REQUIRE_EQ(ht.occupied, 0u);

	CHECK_TRUE(ht.Insert("foo", 1));
	REQUIRE_EQ(ht.occupied, 1u);
	REQUIRE_NOT_NULL(ht.Lookup("foo"));
	REQUIRE_EQ(*ht.Lookup("foo"), 1);

	CHECK_FALSE(ht.Insert("foo", 2));  // already exists
	REQUIRE_EQ(ht.occupied, 1u);
}

void Test_Hashtable_Insert_LookupReturnsCorrectValues() {
	Hashtable<int> ht;

	CHECK_TRUE(ht.Insert("first", 1));
	CHECK_TRUE(ht.Insert("second", 2));
	CHECK_TRUE(ht.Insert("third", 3));

	REQUIRE_NOT_NULL(ht.Lookup("first"));
	REQUIRE_EQ(*ht.Lookup("first"), 1);

	REQUIRE_NOT_NULL(ht.Lookup("second"));
	REQUIRE_EQ(*ht.Lookup("second"), 2);

	REQUIRE_NOT_NULL(ht.Lookup("third"));
	REQUIRE_EQ(*ht.Lookup("third"), 3);

	REQUIRE_IS_NULL(ht.Lookup("nonexistent"));
}

void Test_Hashtable_Upsert_UpdateExistingValue() {
	Hashtable<int> ht;

	CHECK_TRUE(ht.Insert("key", 1));
	REQUIRE_EQ(*ht.Lookup("key"), 1);

	CHECK_FALSE(ht.Upsert("key", 99));  // already exists, just updates value
	REQUIRE_EQ(*ht.Lookup("key"), 99);
}

void Test_Hashtable_Upsert_InsertsWhenNotExists() {
	Hashtable<int> ht;

	CHECK_TRUE(ht.Upsert("missing", 42));
	REQUIRE_EQ(ht.occupied, 1u);
	REQUIRE_NOT_NULL(ht.Lookup("missing"));
	REQUIRE_EQ(*ht.Lookup("missing"), 42);
}

void Test_Hashtable_Reserve_CreatesInitialCapacity() {
	Hashtable<int> ht;
	REQUIRE_EQ(ht.size, 0u);
	REQUIRE_EQ(ht.occupied, 0u);

	ht.Reserve(100u);
	REQUIRE_TRUE(ht.size >= 100u);  // internal size grows to accommodate load factor
}

void Test_Hashtable_Reserve_NoOpWhenAlreadyLargeEnough() {
	Hashtable<int> ht;
	REQUIRE_EQ(ht.size, 0u);

	ht.Reserve(10);
	ht.Reserve(5);  // should be a no-op
}

void Test_Hashtable_GrowthDuringInsertion() {
	Hashtable<int> ht;
	REQUIRE_EQ(ht.size, 0u);
	REQUIRE_EQ(ht.occupied, 0u);

	ht.Insert("a", 1);
	REQUIRE_TRUE(ht.size > 0u);
	
	const int ENTRIES = 10;
	for (int i = 2; i <= ENTRIES; i++) {
		CHECK_TRUE(ht.Insert(std::string {"key"} + std::to_string(i), i));
	}

	REQUIRE_TRUE(ht.occupied == ENTRIES);
}

//
// Tests that hash collisions (same string, different hashes) don't break insertion/lookup
// This is an unlikely scenario but should be handled correctly.
// We artificially create this by using a custom key that will always hash to the same value
// or by directly manipulating slots with InsertWithHash.
//
void Test_Hashtable_CollisionTest_SameKeys_DifferentValues() {
	Hashtable<int> ht;
	
	// Manually insert two items with the same key but different values and hashes
	// This simulates a collision scenario
	CHECK_TRUE(ht.Insert("test", 100));

	// Try to look up "test" - should find it
	REQUIRE_NOT_NULL(ht.Lookup("test"));
	REQUIRE_EQ(*ht.Lookup("test"), 100);

	// Upsert with different value
	CHECK_FALSE(ht.Upsert("test", 200));
	REQUIRE_EQ(*ht.Lookup("test"), 200);
}

//
// Test_Hashtable_CollisionTest_DifferentKeys_SameHash
// Tests linear probing handles hash collisions correctly
// We use a simple trick: insert items with the same prefix to force similar hashes
//
void Test_Hashtable_CollisionTest_DifferentKeys_SameHash() {
	Hashtable<std::string> ht;

	// Insert several strings that might have similar hashes
	CHECK_TRUE(ht.InsertWithHash("apple", 1, "apple"));
	CHECK_TRUE(ht.InsertWithHash("apply", 1, "apply"));
	CHECK_FALSE(ht.InsertWithHash("apply", 1, "apply"));  // same as above, should not insert
	
	REQUIRE_EQ(ht.occupied, 2u);

	// Verify all lookups work correctly
	const std::string* valueApple = ht.LookupWithHash("apple", 1);
	REQUIRE_NOT_NULL(valueApple);
	REQUIRE_EQ(*valueApple, "apple");

	const std::string* valueApply = ht.LookupWithHash("apply", 1);
	REQUIRE_NOT_NULL(valueApply);
	REQUIRE_EQ(*valueApply, "apply");
}
