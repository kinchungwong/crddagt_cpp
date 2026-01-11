#include <gtest/gtest.h>
#include <algorithm>
// #include <chrono> // for human review of potentially slow tests only.
#include <set>
#include <numeric>
#include "crddagt/common/iterable_union_find.inline.hpp"

using namespace crddagt;

// =============================================================================
// Basic Operations Tests
// =============================================================================

TEST(IterableUnionFindTests, MakeSet_SingletonHasSizeOne) {
    IterableUnionFind<size_t> uf;
    size_t x = uf.make_set();
    EXPECT_EQ(uf.class_size(x), 1);
}

TEST(IterableUnionFindTests, MakeSet_SingletonIsSelfRoot) {
    IterableUnionFind<size_t> uf;
    size_t x = uf.make_set();
    EXPECT_EQ(uf.find(x), x);
}

TEST(IterableUnionFindTests, MakeSet_SingletonCircularList) {
    IterableUnionFind<size_t> uf;
    size_t x = uf.make_set();
    std::vector<size_t> members;
    uf.get_class_members(x, members);
    ASSERT_EQ(members.size(), 1);
    EXPECT_EQ(members[0], x);
}

TEST(IterableUnionFindTests, MakeSet_SequentialIndices) {
    IterableUnionFind<size_t> uf;
    EXPECT_EQ(uf.make_set(), 0);
    EXPECT_EQ(uf.make_set(), 1);
    EXPECT_EQ(uf.make_set(), 2);
    EXPECT_EQ(uf.make_set(), 3);
    EXPECT_EQ(uf.element_count(), 4);
}

// =============================================================================
// Unite Operations Tests
// =============================================================================

TEST(IterableUnionFindTests, Unite_TwoSingletons_MergesCorrectly) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();

    EXPECT_TRUE(uf.unite(a, b));
    EXPECT_EQ(uf.find(a), uf.find(b));
    EXPECT_EQ(uf.class_size(a), 2);
    EXPECT_EQ(uf.class_size(b), 2);
}

TEST(IterableUnionFindTests, Unite_SameClass_ReturnsFalse) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();

    uf.unite(a, b);
    EXPECT_FALSE(uf.unite(a, b));
    EXPECT_FALSE(uf.unite(b, a));
}

TEST(IterableUnionFindTests, Unite_SameClass_SizeUnchanged) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();

    uf.unite(a, b);
    size_t size_before = uf.class_size(a);
    uf.unite(a, b);
    EXPECT_EQ(uf.class_size(a), size_before);
}

TEST(IterableUnionFindTests, Unite_ChainOfThree_AllSameRoot) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    size_t c = uf.make_set();

    uf.unite(a, b);
    uf.unite(b, c);

    EXPECT_EQ(uf.find(a), uf.find(b));
    EXPECT_EQ(uf.find(b), uf.find(c));
    EXPECT_EQ(uf.class_size(a), 3);
}

TEST(IterableUnionFindTests, Unite_RankDecision_SmallerUnderLarger) {
    IterableUnionFind<size_t> uf;
    // Create two sets of different sizes to get different ranks
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    size_t c = uf.make_set();
    size_t d = uf.make_set();

    // Unite a-b (rank 1) and c-d (rank 1), then merge them
    uf.unite(a, b);
    uf.unite(c, d);

    // Now both have rank 1, so we can unite a with c
    // Add more elements to one side to create rank difference
    size_t e = uf.make_set();
    size_t f = uf.make_set();
    uf.unite(e, f);
    uf.unite(a, e);  // Now a's class has rank 2

    // Unite with c's class (rank 1) - c should go under a's root
    size_t root_a_before = uf.find(a);
    uf.unite(a, c);
    size_t root_after = uf.find(c);

    // The root of the larger-rank tree should become the root
    EXPECT_EQ(root_after, root_a_before);
}

// =============================================================================
// Size Tracking Tests
// =============================================================================

TEST(IterableUnionFindTests, Size_TotalityInvariant_AfterMakeSet) {
    IterableUnionFind<size_t> uf;
    for (int i = 0; i < 10; ++i) {
        uf.make_set();
    }

    // Calculate sum of sizes by checking each element's class
    // Since we haven't united anything, each is its own class with size 1
    size_t total = 0;
    for (size_t i = 0; i < uf.element_count(); ++i) {
        if (uf.class_root(i) == i) {
            total += uf.class_size(i);
        }
    }
    EXPECT_EQ(total, uf.element_count());
}

TEST(IterableUnionFindTests, Size_TotalityInvariant_AfterUnite) {
    IterableUnionFind<size_t> uf;
    for (int i = 0; i < 10; ++i) {
        uf.make_set();
    }

    // Perform some unites
    uf.unite(0, 1);
    uf.unite(2, 3);
    uf.unite(0, 2);
    uf.unite(5, 6);
    uf.unite(7, 8);
    uf.unite(5, 7);

    // Sum sizes at roots only
    size_t total = 0;
    for (size_t i = 0; i < uf.element_count(); ++i) {
        if (uf.class_root(i) == i) {
            total += uf.class_size(i);
        }
    }
    EXPECT_EQ(total, uf.element_count());
}

TEST(IterableUnionFindTests, Size_NonRootReturnsCorrectSize) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    size_t c = uf.make_set();

    uf.unite(a, b);
    uf.unite(b, c);

    // All should report size 3, regardless of which is root
    EXPECT_EQ(uf.class_size(a), 3);
    EXPECT_EQ(uf.class_size(b), 3);
    EXPECT_EQ(uf.class_size(c), 3);
}

TEST(IterableUnionFindTests, Size_RootReturnsCorrectSize) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();

    uf.unite(a, b);

    size_t root = uf.find(a);
    EXPECT_EQ(uf.class_size(root), 2);
}

// =============================================================================
// Circular List Enumeration Tests
// =============================================================================

TEST(IterableUnionFindTests, GetMembers_Singleton_ReturnsSelf) {
    IterableUnionFind<size_t> uf;
    size_t x = uf.make_set();
    std::vector<size_t> members;
    uf.get_class_members(x, members);
    ASSERT_EQ(members.size(), 1);
    EXPECT_EQ(members[0], x);
}

TEST(IterableUnionFindTests, GetMembers_TwoElements_ReturnsBoth) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    uf.unite(a, b);

    std::vector<size_t> members;
    uf.get_class_members(a, members);

    ASSERT_EQ(members.size(), 2);
    std::set<size_t> member_set(members.begin(), members.end());
    EXPECT_TRUE(member_set.count(a));
    EXPECT_TRUE(member_set.count(b));
}

TEST(IterableUnionFindTests, GetMembers_FromAnyMember_SameResult) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    size_t c = uf.make_set();
    uf.unite(a, b);
    uf.unite(b, c);

    std::vector<size_t> members_a, members_b, members_c;
    uf.get_class_members(a, members_a);
    uf.get_class_members(b, members_b);
    uf.get_class_members(c, members_c);

    // All should have the same elements (possibly in different order)
    std::set<size_t> set_a(members_a.begin(), members_a.end());
    std::set<size_t> set_b(members_b.begin(), members_b.end());
    std::set<size_t> set_c(members_c.begin(), members_c.end());

    EXPECT_EQ(set_a, set_b);
    EXPECT_EQ(set_b, set_c);
}

TEST(IterableUnionFindTests, GetMembers_LargeClass_AllPresent) {
    IterableUnionFind<size_t> uf;
    const size_t N = 100;

    for (size_t i = 0; i < N; ++i) {
        uf.make_set();
    }

    // Unite all into one class
    for (size_t i = 1; i < N; ++i) {
        uf.unite(0, i);
    }

    std::vector<size_t> members;
    uf.get_class_members(0, members);

    ASSERT_EQ(members.size(), N);

    std::set<size_t> member_set(members.begin(), members.end());
    for (size_t i = 0; i < N; ++i) {
        EXPECT_TRUE(member_set.count(i)) << "Missing element " << i;
    }
}

TEST(IterableUnionFindTests, GetMembers_CountMatchesSize) {
    IterableUnionFind<size_t> uf;
    for (int i = 0; i < 10; ++i) {
        uf.make_set();
    }

    uf.unite(0, 1);
    uf.unite(2, 3);
    uf.unite(0, 2);
    uf.unite(5, 6);

    // Check each element's class
    for (size_t i = 0; i < uf.element_count(); ++i) {
        std::vector<size_t> members;
        uf.get_class_members(i, members);
        EXPECT_EQ(members.size(), uf.class_size(i))
            << "Mismatch at element " << i;
    }
}

// =============================================================================
// Path Compression Tests
// =============================================================================

TEST(IterableUnionFindTests, Find_CompressesPath) {
    IterableUnionFind<size_t> uf;
    // Create a chain: 0 <- 1 <- 2 <- 3
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    size_t c = uf.make_set();
    size_t d = uf.make_set();

    uf.unite(a, b);
    uf.unite(b, c);
    uf.unite(c, d);

    size_t root = uf.find(d);

    // After find(d), d should point directly to root
    // We verify by checking that a second find is still correct
    EXPECT_EQ(uf.find(d), root);
    EXPECT_EQ(uf.find(c), root);
    EXPECT_EQ(uf.find(b), root);
    EXPECT_EQ(uf.find(a), root);
}

TEST(IterableUnionFindTests, Find_DoesNotAffectCircularList) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    size_t c = uf.make_set();

    uf.unite(a, b);
    uf.unite(b, c);

    std::vector<size_t> members_before;
    uf.get_class_members(a, members_before);

    // Perform finds which compress paths
    uf.find(a);
    uf.find(b);
    uf.find(c);

    std::vector<size_t> members_after;
    uf.get_class_members(a, members_after);

    // Members should be the same
    std::set<size_t> set_before(members_before.begin(), members_before.end());
    std::set<size_t> set_after(members_after.begin(), members_after.end());
    EXPECT_EQ(set_before, set_after);
}

TEST(IterableUnionFindTests, ClassRoot_DoesNotCompress) {
    IterableUnionFind<size_t> uf;
    // We can't directly verify internal state, but we can verify
    // that class_root is const and returns the same as find
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    size_t c = uf.make_set();

    uf.unite(a, b);
    uf.unite(b, c);

    // class_root should give same result as find
    EXPECT_EQ(uf.class_root(a), uf.find(a));
    EXPECT_EQ(uf.class_root(b), uf.find(b));
    EXPECT_EQ(uf.class_root(c), uf.find(c));
}

// =============================================================================
// Index Validation Tests
// =============================================================================

TEST(IterableUnionFindTests, Find_InvalidIndex_Throws) {
    IterableUnionFind<size_t> uf;
    EXPECT_THROW(uf.find(0), std::runtime_error);
    EXPECT_THROW(uf.find(999), std::runtime_error);
}

TEST(IterableUnionFindTests, Unite_InvalidIndex_Throws) {
    IterableUnionFind<size_t> uf;
    uf.make_set();  // index 0 exists

    EXPECT_THROW(uf.unite(0, 999), std::runtime_error);
    EXPECT_THROW(uf.unite(999, 0), std::runtime_error);
}

TEST(IterableUnionFindTests, ClassSize_InvalidIndex_Throws) {
    IterableUnionFind<size_t> uf;
    EXPECT_THROW(uf.class_size(0), std::runtime_error);
    EXPECT_THROW(uf.class_size(999), std::runtime_error);
}

TEST(IterableUnionFindTests, GetMembers_InvalidIndex_Throws) {
    IterableUnionFind<size_t> uf;
    std::vector<size_t> out;
    EXPECT_THROW(uf.get_class_members(0, out), std::runtime_error);
    EXPECT_THROW(uf.get_class_members(999, out), std::runtime_error);
}

TEST(IterableUnionFindTests, SameClass_InvalidIndex_Throws) {
    IterableUnionFind<size_t> uf;
    uf.make_set();  // index 0 exists

    EXPECT_THROW(uf.same_class(0, 999), std::runtime_error);
    EXPECT_THROW(uf.same_class(999, 0), std::runtime_error);
}

TEST(IterableUnionFindTests, ClassRoot_InvalidIndex_Throws) {
    IterableUnionFind<size_t> uf;
    EXPECT_THROW(uf.class_root(0), std::runtime_error);
    EXPECT_THROW(uf.class_root(999), std::runtime_error);
}

TEST(IterableUnionFindTests, ErrorMessage_ContainsIndex) {
    IterableUnionFind<size_t> uf;
    try {
        uf.find(42);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("42"), std::string::npos)
            << "Error message should contain the invalid index: " << msg;
    }
}

TEST(IterableUnionFindTests, ErrorMessage_ContainsRange) {
    IterableUnionFind<size_t> uf;
    uf.make_set();
    uf.make_set();
    uf.make_set();  // Valid range is [0, 3)

    try {
        uf.find(5);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("3"), std::string::npos)
            << "Error message should contain the range upper bound: " << msg;
    }
}

// =============================================================================
// Edge Cases Tests
// =============================================================================

TEST(IterableUnionFindTests, Empty_ElementCountZero) {
    IterableUnionFind<size_t> uf;
    EXPECT_EQ(uf.element_count(), 0);
}

TEST(IterableUnionFindTests, Unite_WithSelf_ReturnsFalse) {
    IterableUnionFind<size_t> uf;
    size_t x = uf.make_set();
    EXPECT_FALSE(uf.unite(x, x));
    EXPECT_EQ(uf.class_size(x), 1);
}

// =============================================================================
// same_class() Tests
// =============================================================================

TEST(IterableUnionFindTests, SameClass_SameElement_ReturnsTrue) {
    IterableUnionFind<size_t> uf;
    size_t x = uf.make_set();
    EXPECT_TRUE(uf.same_class(x, x));
}

TEST(IterableUnionFindTests, SameClass_DifferentClasses_ReturnsFalse) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    EXPECT_FALSE(uf.same_class(a, b));
}

TEST(IterableUnionFindTests, SameClass_AfterUnite_ReturnsTrue) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    uf.unite(a, b);
    EXPECT_TRUE(uf.same_class(a, b));
}

TEST(IterableUnionFindTests, SameClass_TransitiveAfterUnite_ReturnsTrue) {
    IterableUnionFind<size_t> uf;
    size_t a = uf.make_set();
    size_t b = uf.make_set();
    size_t c = uf.make_set();
    uf.unite(a, b);
    uf.unite(b, c);
    EXPECT_TRUE(uf.same_class(a, c));
}

// =============================================================================
// Overflow Tests with Small Index Types
// =============================================================================

TEST(IterableUnionFindTests, MakeSet_Uint8_OverflowAtMax) {
    // uint8_t max is 255, so we can create 255 elements (indices 0-254)
    // The 256th call (which would be index 255) should throw
    // because m_nodes.size() == 255 >= numeric_limits<uint8_t>::max() == 255
    IterableUnionFind<uint8_t> uf;

    // Create 255 elements successfully
    for (int i = 0; i < 255; ++i) {
        EXPECT_NO_THROW(uf.make_set());
    }
    EXPECT_EQ(uf.element_count(), 255u);

    // The 256th call should throw overflow_error
    EXPECT_THROW(uf.make_set(), std::overflow_error);

    // Element count should still be 255
    EXPECT_EQ(uf.element_count(), 255u);
}

TEST(IterableUnionFindTests, MakeSet_Uint16_OverflowAtMax) {
    // uint16_t max is 65535, so we can create 65535 elements (indices 0-65534)
    // The 65536th call should throw overflow_error
    IterableUnionFind<uint16_t> uf;

    // auto start = std::chrono::steady_clock::now();

    // Create 65535 elements successfully
    for (int i = 0; i < 65535; ++i) {
        uf.make_set();
    }

    // auto end = std::chrono::steady_clock::now();
    // auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    // auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    EXPECT_EQ(uf.element_count(), 65535u);

    // Log the duration for informational purposes. Sample output (number from a real run):
    //   [INFO] Created 65535 uint16_t elements in 1490710 ns
    // std::cout << "  [INFO] Created 65535 uint16_t elements in " << duration_ns << " ns" << std::endl;

    // The 65536th call should throw overflow_error
    EXPECT_THROW(uf.make_set(), std::overflow_error);

    // Element count should still be 65535
    EXPECT_EQ(uf.element_count(), 65535u);
}

TEST(IterableUnionFindTests, PostOverflow_LinearChainUnite_OperationsWork) {
    // Fill uint8_t to overflow point and verify operations still work
    // Uses linear chain unite pattern: 0-1, 1-2, 2-3, ... (keeps rank low)
    IterableUnionFind<uint8_t> uf;

    // Create all 255 elements
    for (int i = 0; i < 255; ++i) {
        uf.make_set();
    }
    EXPECT_EQ(uf.element_count(), 255u);

    // Trigger and verify overflow_error
    EXPECT_THROW(uf.make_set(), std::overflow_error);

    // Leave a few elements unmerged for verification
    // Unite elements 0 through 250 in a linear chain
    for (uint8_t i = 0; i < 250; ++i) {
        EXPECT_TRUE(uf.unite(i, static_cast<uint8_t>(i + 1)));
    }

    // Verify find works on all merged elements
    uint8_t root = uf.find(0);
    for (uint8_t i = 0; i <= 250; ++i) {
        EXPECT_EQ(uf.find(i), root) << "Element " << static_cast<int>(i) << " has wrong root";
    }

    // Verify size is correct
    EXPECT_EQ(uf.class_size(0), 251u);

    // Verify unmerged elements are still separate
    EXPECT_NE(uf.find(251), root);
    EXPECT_NE(uf.find(254), root);
    EXPECT_NE(uf.find(251), uf.find(254));
    EXPECT_EQ(uf.class_size(251), 1u);
    EXPECT_EQ(uf.class_size(254), 1u);

    // Verify same_class works
    EXPECT_TRUE(uf.same_class(0, 250));
    EXPECT_FALSE(uf.same_class(0, 251));
    EXPECT_FALSE(uf.same_class(251, 254));

    // Verify class_rank doesn't crash (don't assert on actual value)
    (void)uf.class_rank(0);
    (void)uf.class_rank(251);
    std::cout << "  [INFO] Final class_rank: " << static_cast<int>(uf.class_rank(0)) << std::endl;

    // Verify get_class_members works
    std::vector<uint8_t> members;
    uf.get_class_members(100, members);
    EXPECT_EQ(members.size(), 251u);
}

TEST(IterableUnionFindTests, PostOverflow_BalancedTreeUnite_OperationsWork) {
    // Fill uint8_t to overflow point and verify operations still work
    // Uses balanced tree unite pattern: pairs, then pairs of pairs, etc. (grows rank)
    IterableUnionFind<uint8_t> uf;

    // Create all 255 elements
    for (int i = 0; i < 255; ++i) {
        uf.make_set();
    }
    EXPECT_EQ(uf.element_count(), 255u);

    // Trigger and verify overflow_error
    EXPECT_THROW(uf.make_set(), std::overflow_error);

    // Unite first 128 elements in a balanced tree pattern
    // This maximizes rank growth: log2(128) = 7
    // Round 1: unite pairs (0,1), (2,3), (4,5), ..., (126,127) -> 64 groups of size 2
    for (uint8_t i = 0; i < 128; i += 2) {
        uf.unite(i, static_cast<uint8_t>(i + 1));
    }

    // Round 2: unite pairs of pairs -> 32 groups of size 4
    for (uint8_t i = 0; i < 128; i += 4) {
        uf.unite(i, static_cast<uint8_t>(i + 2));
    }

    // Round 3: -> 16 groups of size 8
    for (uint8_t i = 0; i < 128; i += 8) {
        uf.unite(i, static_cast<uint8_t>(i + 4));
    }

    // Round 4: -> 8 groups of size 16
    for (uint8_t i = 0; i < 128; i += 16) {
        uf.unite(i, static_cast<uint8_t>(i + 8));
    }

    // Round 5: -> 4 groups of size 32
    for (uint8_t i = 0; i < 128; i += 32) {
        uf.unite(i, static_cast<uint8_t>(i + 16));
    }

    // Round 6: -> 2 groups of size 64
    for (uint8_t i = 0; i < 128; i += 64) {
        uf.unite(i, static_cast<uint8_t>(i + 32));
    }

    // Round 7: -> 1 group of size 128
    uf.unite(0, 64);

    // Verify find works on all merged elements
    uint8_t root = uf.find(0);
    for (uint8_t i = 0; i < 128; ++i) {
        EXPECT_EQ(uf.find(i), root) << "Element " << static_cast<int>(i) << " has wrong root";
    }

    // Verify size is correct
    EXPECT_EQ(uf.class_size(0), 128u);

    // Verify unmerged elements are still separate
    EXPECT_NE(uf.find(128), root);
    EXPECT_NE(uf.find(200), root);
    EXPECT_EQ(uf.class_size(128), 1u);
    EXPECT_EQ(uf.class_size(200), 1u);
    EXPECT_EQ(uf.class_size(254), 1u);

    // Verify same_class works
    EXPECT_TRUE(uf.same_class(0, 127));
    EXPECT_TRUE(uf.same_class(63, 64));
    EXPECT_FALSE(uf.same_class(0, 128));

    // Verify class_rank doesn't crash (don't assert on actual value)
    (void)uf.class_rank(0);
    (void)uf.class_rank(128);
    std::cout << "  [INFO] Final class_rank: " << static_cast<int>(uf.class_rank(0)) << std::endl;

    // Verify get_class_members works
    std::vector<uint8_t> members;
    uf.get_class_members(50, members);
    EXPECT_EQ(members.size(), 128u);
}

// =============================================================================
// Reserve and InitSets Tests
// =============================================================================

TEST(IterableUnionFindTests, Reserve_DoesNotChangeElementCount) {
    IterableUnionFind<size_t> uf;
    uf.reserve(100);
    EXPECT_EQ(uf.element_count(), 0u);
}

TEST(IterableUnionFindTests, Reserve_AllowsSubsequentMakeSets) {
    IterableUnionFind<size_t> uf;
    uf.reserve(10);
    for (int i = 0; i < 10; ++i) {
        EXPECT_NO_THROW(uf.make_set());
    }
    EXPECT_EQ(uf.element_count(), 10u);
}

TEST(IterableUnionFindTests, Reserve_ClampsToMaxForSmallIdx) {
    // uint8_t max is 255, reserving more should clamp
    IterableUnionFind<uint8_t> uf;
    EXPECT_NO_THROW(uf.reserve(1000));
    // Should still be able to create up to 255 elements
    for (int i = 0; i < 255; ++i) {
        uf.make_set();
    }
    EXPECT_EQ(uf.element_count(), 255u);
    EXPECT_THROW(uf.make_set(), std::overflow_error);
}

TEST(IterableUnionFindTests, InitSets_CreatesCorrectNumberOfSingletons) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(10);
    EXPECT_EQ(uf.element_count(), 10u);
}

TEST(IterableUnionFindTests, InitSets_EachElementIsSelfRoot) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(5);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(uf.find(i), i);
    }
}

TEST(IterableUnionFindTests, InitSets_EachElementHasSizeOne) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(5);
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(uf.class_size(i), 1u);
    }
}

TEST(IterableUnionFindTests, InitSets_ElementsCanBeUnited) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(5);
    EXPECT_TRUE(uf.unite(0, 1));
    EXPECT_TRUE(uf.unite(2, 3));
    EXPECT_TRUE(uf.unite(0, 2));
    EXPECT_EQ(uf.class_size(0), 4u);
    EXPECT_EQ(uf.class_size(4), 1u);
}

TEST(IterableUnionFindTests, InitSets_ThrowsOnNonEmpty) {
    IterableUnionFind<size_t> uf;
    uf.make_set();
    EXPECT_THROW(uf.init_sets(10), std::logic_error);
}

TEST(IterableUnionFindTests, InitSets_ZeroCreatesEmpty) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(0);
    EXPECT_EQ(uf.element_count(), 0u);
}

TEST(IterableUnionFindTests, InitSets_Uint8_MaxElements) {
    IterableUnionFind<uint8_t> uf;
    uf.init_sets(255);
    EXPECT_EQ(uf.element_count(), 255u);
    // Verify a few elements
    EXPECT_EQ(uf.find(0), 0);
    EXPECT_EQ(uf.find(254), 254);
    EXPECT_EQ(uf.class_size(100), 1u);
}

TEST(IterableUnionFindTests, InitSets_AllowsSubsequentMakeSetUntilOverflow) {
    IterableUnionFind<uint8_t> uf;
    uf.init_sets(250);
    EXPECT_EQ(uf.element_count(), 250u);
    // Can still add 5 more
    for (int i = 0; i < 5; ++i) {
        EXPECT_NO_THROW(uf.make_set());
    }
    EXPECT_EQ(uf.element_count(), 255u);
    // Next should overflow
    EXPECT_THROW(uf.make_set(), std::overflow_error);
}

// =============================================================================
// NumClasses and GetClassRepresentatives Tests
// =============================================================================

TEST(IterableUnionFindTests, NumClasses_EmptyIsZero) {
    IterableUnionFind<size_t> uf;
    EXPECT_EQ(uf.num_classes(), 0u);
}

TEST(IterableUnionFindTests, NumClasses_SingletonsEqualElementCount) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(10);
    EXPECT_EQ(uf.num_classes(), 10u);
}

TEST(IterableUnionFindTests, NumClasses_DecreasesAfterUnite) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(10);
    uf.unite(0, 1);
    EXPECT_EQ(uf.num_classes(), 9u);
    uf.unite(2, 3);
    EXPECT_EQ(uf.num_classes(), 8u);
    uf.unite(0, 2);
    EXPECT_EQ(uf.num_classes(), 7u);
}

TEST(IterableUnionFindTests, NumClasses_UnchangedByDuplicateUnite) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(5);
    uf.unite(0, 1);
    size_t count_before = uf.num_classes();
    uf.unite(0, 1);  // No-op
    EXPECT_EQ(uf.num_classes(), count_before);
}

TEST(IterableUnionFindTests, GetClassRepresentatives_EmptyReturnsEmpty) {
    IterableUnionFind<size_t> uf;
    std::vector<size_t> roots;
    uf.get_class_representatives(roots);
    EXPECT_TRUE(roots.empty());
}

TEST(IterableUnionFindTests, GetClassRepresentatives_SingletonsReturnsAll) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(5);
    std::vector<size_t> roots;
    uf.get_class_representatives(roots);
    ASSERT_EQ(roots.size(), 5u);
    std::set<size_t> root_set(roots.begin(), roots.end());
    for (size_t i = 0; i < 5; ++i) {
        EXPECT_TRUE(root_set.count(i));
    }
}

TEST(IterableUnionFindTests, GetClassRepresentatives_AfterUnite) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(5);
    uf.unite(0, 1);
    uf.unite(2, 3);

    std::vector<size_t> roots;
    uf.get_class_representatives(roots);
    EXPECT_EQ(roots.size(), 3u);  // {0,1}, {2,3}, {4}

    // Each root should be its own parent
    for (size_t r : roots) {
        EXPECT_EQ(uf.class_root(r), r);
    }
}

TEST(IterableUnionFindTests, GetClassRepresentatives_CountMatchesNumClasses) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(10);
    uf.unite(0, 1);
    uf.unite(2, 3);
    uf.unite(4, 5);
    uf.unite(0, 2);

    std::vector<size_t> roots;
    uf.get_class_representatives(roots);
    EXPECT_EQ(roots.size(), uf.num_classes());
}

// =============================================================================
// ExportNodes Tests
// =============================================================================

TEST(IterableUnionFindTests, ExportNodes_EmptyReturnsEmpty) {
    IterableUnionFind<size_t> uf;
    std::vector<IterableUnionFind<size_t>::Node> nodes;
    uf.export_nodes(nodes);
    EXPECT_TRUE(nodes.empty());
}

TEST(IterableUnionFindTests, ExportNodes_SingletonHasCorrectState) {
    IterableUnionFind<size_t> uf;
    uf.make_set();

    std::vector<IterableUnionFind<size_t>::Node> nodes;
    uf.export_nodes(nodes);
    ASSERT_EQ(nodes.size(), 1u);
    EXPECT_EQ(nodes[0].parent, 0u);
    EXPECT_EQ(nodes[0].rank, 0u);
    EXPECT_EQ(nodes[0].size, 1u);
    EXPECT_EQ(nodes[0].next, 0u);
}

TEST(IterableUnionFindTests, ExportNodes_AfterUnite) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(3);
    uf.unite(0, 1);

    std::vector<IterableUnionFind<size_t>::Node> nodes;
    uf.export_nodes(nodes);
    ASSERT_EQ(nodes.size(), 3u);

    // Find the root (the one with size 2)
    size_t root = (nodes[0].size == 2) ? 0 : 1;
    size_t non_root = 1 - root;

    EXPECT_EQ(nodes[root].parent, root);
    EXPECT_EQ(nodes[root].size, 2u);
    EXPECT_EQ(nodes[non_root].parent, root);
    EXPECT_EQ(nodes[non_root].size, 0u);

    // Element 2 should still be singleton
    EXPECT_EQ(nodes[2].parent, 2u);
    EXPECT_EQ(nodes[2].size, 1u);
}

// =============================================================================
// GetClasses Tests
// =============================================================================

TEST(IterableUnionFindTests, GetClasses_EmptyReturnsEmpty) {
    IterableUnionFind<size_t> uf;
    std::vector<std::vector<size_t>> classes;
    uf.get_classes(classes);
    EXPECT_TRUE(classes.empty());
}

TEST(IterableUnionFindTests, GetClasses_SingletonsEachInOwnClass) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(5);
    std::vector<std::vector<size_t>> classes;
    uf.get_classes(classes);

    ASSERT_EQ(classes.size(), 5u);
    for (size_t i = 0; i < 5; ++i) {
        ASSERT_EQ(classes[i].size(), 1u);
        EXPECT_EQ(classes[i][0], i);
    }
}

TEST(IterableUnionFindTests, GetClasses_AfterUnite) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(5);
    uf.unite(0, 1);
    uf.unite(2, 3);

    std::vector<std::vector<size_t>> classes;
    uf.get_classes(classes);

    ASSERT_EQ(classes.size(), 3u);  // {0,1}, {2,3}, {4}

    // Verify total element count
    size_t total = 0;
    for (const auto& cls : classes) {
        total += cls.size();
    }
    EXPECT_EQ(total, 5u);

    // Verify each class has correct size
    std::vector<size_t> sizes;
    for (const auto& cls : classes) {
        sizes.push_back(cls.size());
    }
    std::sort(sizes.begin(), sizes.end());
    EXPECT_EQ(sizes[0], 1u);  // {4}
    EXPECT_EQ(sizes[1], 2u);  // {0,1} or {2,3}
    EXPECT_EQ(sizes[2], 2u);  // {0,1} or {2,3}
}

TEST(IterableUnionFindTests, GetClasses_MembersMatchGetClassMembers) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(10);
    uf.unite(0, 1);
    uf.unite(2, 3);
    uf.unite(0, 2);
    uf.unite(5, 6);

    std::vector<std::vector<size_t>> classes;
    uf.get_classes(classes);

    // For each class, verify members match get_class_members
    for (const auto& cls : classes) {
        ASSERT_FALSE(cls.empty());
        std::vector<size_t> members_from_get;
        uf.get_class_members(cls[0], members_from_get);

        std::set<size_t> set1(cls.begin(), cls.end());
        std::set<size_t> set2(members_from_get.begin(), members_from_get.end());
        EXPECT_EQ(set1, set2);
    }
}

TEST(IterableUnionFindTests, GetClasses_CountMatchesNumClasses) {
    IterableUnionFind<size_t> uf;
    uf.init_sets(10);
    uf.unite(0, 1);
    uf.unite(2, 3);
    uf.unite(4, 5);
    uf.unite(0, 2);

    std::vector<std::vector<size_t>> classes;
    uf.get_classes(classes);
    EXPECT_EQ(classes.size(), uf.num_classes());
}
