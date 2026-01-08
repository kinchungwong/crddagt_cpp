/**
 * @file opk_unique_list_tests.cpp
 * Unit tests for crddagt::OpkUniqueList
 */
#include <gtest/gtest.h>
#include "crddagt/common/opk_unique_list.hpp"

#include <limits>
#include <string>
#include <vector>

using namespace crddagt;

// ============================================================================
// npos constant
// ============================================================================

TEST(OpkUniqueListTests, Npos_IsSizeMax)
{
    EXPECT_EQ(OpkUniqueList<int>::npos, std::numeric_limits<std::size_t>::max());
}

TEST(OpkUniqueListTests, Npos_IsConstexpr)
{
    // Compile-time check: npos can be used in constexpr context
    constexpr std::size_t val = OpkUniqueList<int>::npos;
    EXPECT_EQ(val, ~static_cast<std::size_t>(0));
}

// ============================================================================
// Construction
// ============================================================================

TEST(OpkUniqueListTests, Construction_DefaultIsEmpty)
{
    OpkUniqueList<int> list;
    EXPECT_EQ(list.size(), 0u);
}

// ============================================================================
// Insert - basic functionality
// ============================================================================

TEST(OpkUniqueListTests, Insert_FirstElementReturnsZero)
{
    int obj = 42;
    OpkUniqueList<int> list;
    std::size_t idx = list.insert(OpaquePtrKey<int>(&obj));
    EXPECT_EQ(idx, 0u);
}

TEST(OpkUniqueListTests, Insert_SequentialInsertsReturnIncrementingIndices)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    EXPECT_EQ(list.insert(OpaquePtrKey<int>(&a)), 0u);
    EXPECT_EQ(list.insert(OpaquePtrKey<int>(&b)), 1u);
    EXPECT_EQ(list.insert(OpaquePtrKey<int>(&c)), 2u);
}

TEST(OpkUniqueListTests, Insert_DuplicateReturnsExistingIndex)
{
    int obj = 42;
    OpkUniqueList<int> list;

    std::size_t first = list.insert(OpaquePtrKey<int>(&obj));
    std::size_t second = list.insert(OpaquePtrKey<int>(&obj));

    EXPECT_EQ(first, second);
    EXPECT_EQ(list.size(), 1u);  // No new element added
}

TEST(OpkUniqueListTests, Insert_DuplicateAmongMultiple)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));  // index 0
    list.insert(OpaquePtrKey<int>(&b));  // index 1
    list.insert(OpaquePtrKey<int>(&c));  // index 2

    // Re-insert middle element
    std::size_t idx = list.insert(OpaquePtrKey<int>(&b));
    EXPECT_EQ(idx, 1u);
    EXPECT_EQ(list.size(), 3u);
}

TEST(OpkUniqueListTests, Insert_NullKeyThrows)
{
    OpkUniqueList<int> list;
    OpaquePtrKey<int> null_key(static_cast<int*>(nullptr));

    EXPECT_THROW(list.insert(null_key), std::invalid_argument);
}

TEST(OpkUniqueListTests, Insert_NullKeyDoesNotModifyList)
{
    int obj = 42;
    OpkUniqueList<int> list;
    list.insert(OpaquePtrKey<int>(&obj));

    OpaquePtrKey<int> null_key(static_cast<int*>(nullptr));
    try {
        list.insert(null_key);
    } catch (const std::invalid_argument&) {
        // Expected
    }

    EXPECT_EQ(list.size(), 1u);  // List unchanged
}

TEST(OpkUniqueListTests, Insert_ExpiredWeakPtrThrows)
{
    OpkUniqueList<int> list;
    std::weak_ptr<int> expired;
    {
        auto sp = std::make_shared<int>(42);
        expired = sp;
    }
    // Now expired weak_ptr produces null key
    OpaquePtrKey<int> key(expired);
    EXPECT_THROW(list.insert(key), std::invalid_argument);
}

// ============================================================================
// Find
// ============================================================================

TEST(OpkUniqueListTests, Find_ExistingKeyReturnsCorrectIndex)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));
    list.insert(OpaquePtrKey<int>(&b));
    list.insert(OpaquePtrKey<int>(&c));

    EXPECT_EQ(list.find(OpaquePtrKey<int>(&b)), 1u);
}

TEST(OpkUniqueListTests, Find_NonExistingKeyReturnsNpos)
{
    int a = 1, b = 2;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));

    EXPECT_EQ(list.find(OpaquePtrKey<int>(&b)), OpkUniqueList<int>::npos);
}

TEST(OpkUniqueListTests, Find_EmptyListReturnsNpos)
{
    int obj = 42;
    OpkUniqueList<int> list;

    EXPECT_EQ(list.find(OpaquePtrKey<int>(&obj)), OpkUniqueList<int>::npos);
}

TEST(OpkUniqueListTests, Find_NullKeyReturnsNpos)
{
    int obj = 42;
    OpkUniqueList<int> list;
    list.insert(OpaquePtrKey<int>(&obj));

    OpaquePtrKey<int> null_key(static_cast<int*>(nullptr));
    // find is noexcept, so it should return npos for null key
    EXPECT_EQ(list.find(null_key), OpkUniqueList<int>::npos);
}

TEST(OpkUniqueListTests, Find_IsNoexcept)
{
    OpkUniqueList<int> list;
    int obj = 42;
    OpaquePtrKey<int> key(&obj);

    static_assert(noexcept(list.find(key)), "find must be noexcept");
}

// ============================================================================
// At
// ============================================================================

TEST(OpkUniqueListTests, At_ValidIndexReturnsCorrectKey)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));
    list.insert(OpaquePtrKey<int>(&b));
    list.insert(OpaquePtrKey<int>(&c));

    EXPECT_EQ(list.at(0), OpaquePtrKey<int>(&a));
    EXPECT_EQ(list.at(1), OpaquePtrKey<int>(&b));
    EXPECT_EQ(list.at(2), OpaquePtrKey<int>(&c));
}

TEST(OpkUniqueListTests, At_InvalidIndexThrows)
{
    int obj = 42;
    OpkUniqueList<int> list;
    list.insert(OpaquePtrKey<int>(&obj));

    EXPECT_THROW(list.at(1), std::out_of_range);
}

TEST(OpkUniqueListTests, At_EmptyListThrows)
{
    OpkUniqueList<int> list;
    EXPECT_THROW(list.at(0), std::out_of_range);
}

TEST(OpkUniqueListTests, At_NposIndexThrows)
{
    int obj = 42;
    OpkUniqueList<int> list;
    list.insert(OpaquePtrKey<int>(&obj));

    EXPECT_THROW(list.at(OpkUniqueList<int>::npos), std::out_of_range);
}

// ============================================================================
// Size
// ============================================================================

TEST(OpkUniqueListTests, Size_EmptyListIsZero)
{
    OpkUniqueList<int> list;
    EXPECT_EQ(list.size(), 0u);
}

TEST(OpkUniqueListTests, Size_IncreasesAfterInsert)
{
    int a = 1, b = 2;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));
    EXPECT_EQ(list.size(), 1u);

    list.insert(OpaquePtrKey<int>(&b));
    EXPECT_EQ(list.size(), 2u);
}

TEST(OpkUniqueListTests, Size_UnchangedAfterDuplicateInsert)
{
    int obj = 42;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&obj));
    EXPECT_EQ(list.size(), 1u);

    list.insert(OpaquePtrKey<int>(&obj));
    EXPECT_EQ(list.size(), 1u);  // Still 1
}

TEST(OpkUniqueListTests, Size_IsNoexcept)
{
    OpkUniqueList<int> list;
    static_assert(noexcept(list.size()), "size must be noexcept");
}

// ============================================================================
// Enumerate
// ============================================================================

TEST(OpkUniqueListTests, Enumerate_EmptyListNoCallbacks)
{
    OpkUniqueList<int> list;
    int call_count = 0;

    list.enumerate([&](std::size_t, const OpaquePtrKey<int>&) {
        ++call_count;
    });

    EXPECT_EQ(call_count, 0);
}

TEST(OpkUniqueListTests, Enumerate_VisitsAllElements)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));
    list.insert(OpaquePtrKey<int>(&b));
    list.insert(OpaquePtrKey<int>(&c));

    int call_count = 0;
    list.enumerate([&](std::size_t, const OpaquePtrKey<int>&) {
        ++call_count;
    });

    EXPECT_EQ(call_count, 3);
}

TEST(OpkUniqueListTests, Enumerate_PreservesInsertionOrder)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));
    list.insert(OpaquePtrKey<int>(&b));
    list.insert(OpaquePtrKey<int>(&c));

    std::vector<OpaquePtrKey<int>> visited;
    list.enumerate([&](std::size_t, const OpaquePtrKey<int>& key) {
        visited.push_back(key);
    });

    EXPECT_EQ(visited.size(), 3u);
    EXPECT_EQ(visited[0], OpaquePtrKey<int>(&a));
    EXPECT_EQ(visited[1], OpaquePtrKey<int>(&b));
    EXPECT_EQ(visited[2], OpaquePtrKey<int>(&c));
}

TEST(OpkUniqueListTests, Enumerate_IndicesAreCorrect)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));
    list.insert(OpaquePtrKey<int>(&b));
    list.insert(OpaquePtrKey<int>(&c));

    std::vector<std::size_t> indices;
    list.enumerate([&](std::size_t idx, const OpaquePtrKey<int>&) {
        indices.push_back(idx);
    });

    EXPECT_EQ(indices.size(), 3u);
    EXPECT_EQ(indices[0], 0u);
    EXPECT_EQ(indices[1], 1u);
    EXPECT_EQ(indices[2], 2u);
}

TEST(OpkUniqueListTests, Enumerate_KeysMatchAt)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));
    list.insert(OpaquePtrKey<int>(&b));
    list.insert(OpaquePtrKey<int>(&c));

    list.enumerate([&](std::size_t idx, const OpaquePtrKey<int>& key) {
        EXPECT_EQ(key, list.at(idx));
    });
}

// ============================================================================
// Insertion Order Preservation
// ============================================================================

TEST(OpkUniqueListTests, InsertionOrder_PreservedAcrossMultipleInserts)
{
    int values[5] = {10, 20, 30, 40, 50};
    OpkUniqueList<int> list;

    for (int& v : values) {
        list.insert(OpaquePtrKey<int>(&v));
    }

    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(list.at(i), OpaquePtrKey<int>(&values[i]));
    }
}

TEST(OpkUniqueListTests, InsertionOrder_UnaffectedByDuplicates)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));
    list.insert(OpaquePtrKey<int>(&b));
    list.insert(OpaquePtrKey<int>(&a));  // duplicate
    list.insert(OpaquePtrKey<int>(&c));
    list.insert(OpaquePtrKey<int>(&b));  // duplicate

    EXPECT_EQ(list.size(), 3u);
    EXPECT_EQ(list.at(0), OpaquePtrKey<int>(&a));
    EXPECT_EQ(list.at(1), OpaquePtrKey<int>(&b));
    EXPECT_EQ(list.at(2), OpaquePtrKey<int>(&c));
}

// ============================================================================
// Index-Key Consistency (Invariants)
// ============================================================================

TEST(OpkUniqueListTests, Invariant_FindOfAtReturnsIndex)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    list.insert(OpaquePtrKey<int>(&a));
    list.insert(OpaquePtrKey<int>(&b));
    list.insert(OpaquePtrKey<int>(&c));

    for (std::size_t i = 0; i < list.size(); ++i) {
        EXPECT_EQ(list.find(list.at(i)), i);
    }
}

TEST(OpkUniqueListTests, Invariant_AtOfInsertReturnsKey)
{
    int obj = 42;
    OpkUniqueList<int> list;

    OpaquePtrKey<int> key(&obj);
    std::size_t idx = list.insert(key);

    EXPECT_EQ(list.at(idx), key);
}

TEST(OpkUniqueListTests, Invariant_AtOfFindReturnsKey)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> list;

    OpaquePtrKey<int> key_a(&a);
    OpaquePtrKey<int> key_b(&b);
    OpaquePtrKey<int> key_c(&c);

    list.insert(key_a);
    list.insert(key_b);
    list.insert(key_c);

    EXPECT_EQ(list.at(list.find(key_a)), key_a);
    EXPECT_EQ(list.at(list.find(key_b)), key_b);
    EXPECT_EQ(list.at(list.find(key_c)), key_c);
}

// ============================================================================
// Value Semantics
// ============================================================================

TEST(OpkUniqueListTests, ValueSemantics_CopyConstruction)
{
    int a = 1, b = 2;
    OpkUniqueList<int> original;
    original.insert(OpaquePtrKey<int>(&a));
    original.insert(OpaquePtrKey<int>(&b));

    OpkUniqueList<int> copy(original);

    EXPECT_EQ(copy.size(), 2u);
    EXPECT_EQ(copy.at(0), OpaquePtrKey<int>(&a));
    EXPECT_EQ(copy.at(1), OpaquePtrKey<int>(&b));
}

TEST(OpkUniqueListTests, ValueSemantics_CopyIsIndependent)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> original;
    original.insert(OpaquePtrKey<int>(&a));
    original.insert(OpaquePtrKey<int>(&b));

    OpkUniqueList<int> copy(original);
    copy.insert(OpaquePtrKey<int>(&c));

    EXPECT_EQ(original.size(), 2u);  // Original unchanged
    EXPECT_EQ(copy.size(), 3u);
}

TEST(OpkUniqueListTests, ValueSemantics_CopyAssignment)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> original;
    original.insert(OpaquePtrKey<int>(&a));
    original.insert(OpaquePtrKey<int>(&b));

    OpkUniqueList<int> target;
    target.insert(OpaquePtrKey<int>(&c));

    target = original;

    EXPECT_EQ(target.size(), 2u);
    EXPECT_EQ(target.at(0), OpaquePtrKey<int>(&a));
    EXPECT_EQ(target.at(1), OpaquePtrKey<int>(&b));
}

TEST(OpkUniqueListTests, ValueSemantics_MoveConstruction)
{
    int a = 1, b = 2;
    OpkUniqueList<int> original;
    original.insert(OpaquePtrKey<int>(&a));
    original.insert(OpaquePtrKey<int>(&b));

    OpkUniqueList<int> moved(std::move(original));

    EXPECT_EQ(moved.size(), 2u);
    EXPECT_EQ(moved.at(0), OpaquePtrKey<int>(&a));
    EXPECT_EQ(moved.at(1), OpaquePtrKey<int>(&b));
}

TEST(OpkUniqueListTests, ValueSemantics_MoveAssignment)
{
    int a = 1, b = 2, c = 3;
    OpkUniqueList<int> original;
    original.insert(OpaquePtrKey<int>(&a));
    original.insert(OpaquePtrKey<int>(&b));

    OpkUniqueList<int> target;
    target.insert(OpaquePtrKey<int>(&c));

    target = std::move(original);

    EXPECT_EQ(target.size(), 2u);
    EXPECT_EQ(target.at(0), OpaquePtrKey<int>(&a));
    EXPECT_EQ(target.at(1), OpaquePtrKey<int>(&b));
}

// ============================================================================
// Type Traits
// ============================================================================

TEST(OpkUniqueListTests, TypeTraits_IsDefaultConstructible)
{
    static_assert(std::is_default_constructible_v<OpkUniqueList<int>>,
        "OpkUniqueList must be default constructible");
}

TEST(OpkUniqueListTests, TypeTraits_IsCopyConstructible)
{
    static_assert(std::is_copy_constructible_v<OpkUniqueList<int>>,
        "OpkUniqueList must be copy constructible");
}

TEST(OpkUniqueListTests, TypeTraits_IsMoveConstructible)
{
    static_assert(std::is_move_constructible_v<OpkUniqueList<int>>,
        "OpkUniqueList must be move constructible");
}

TEST(OpkUniqueListTests, TypeTraits_IsCopyAssignable)
{
    static_assert(std::is_copy_assignable_v<OpkUniqueList<int>>,
        "OpkUniqueList must be copy assignable");
}

TEST(OpkUniqueListTests, TypeTraits_IsMoveAssignable)
{
    static_assert(std::is_move_assignable_v<OpkUniqueList<int>>,
        "OpkUniqueList must be move assignable");
}

// ============================================================================
// Different Template Types
// ============================================================================

TEST(OpkUniqueListTests, DifferentTypes_WorksWithCustomType)
{
    struct CustomType { int value; };

    CustomType a{1}, b{2};
    OpkUniqueList<CustomType> list;

    list.insert(OpaquePtrKey<CustomType>(&a));
    list.insert(OpaquePtrKey<CustomType>(&b));

    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(list.find(OpaquePtrKey<CustomType>(&a)), 0u);
    EXPECT_EQ(list.find(OpaquePtrKey<CustomType>(&b)), 1u);
}

TEST(OpkUniqueListTests, DifferentTypes_WorksWithString)
{
    std::string a = "hello", b = "world";
    OpkUniqueList<std::string> list;

    list.insert(OpaquePtrKey<std::string>(&a));
    list.insert(OpaquePtrKey<std::string>(&b));

    EXPECT_EQ(list.size(), 2u);
}

// ============================================================================
// Smart Pointer Integration
// ============================================================================

TEST(OpkUniqueListTests, SmartPointer_SharedPtrInsert)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);

    OpkUniqueList<int> list;
    list.insert(OpaquePtrKey<int>(a));
    list.insert(OpaquePtrKey<int>(b));

    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(list.find(OpaquePtrKey<int>(a)), 0u);
    EXPECT_EQ(list.find(OpaquePtrKey<int>(b)), 1u);
}

TEST(OpkUniqueListTests, SmartPointer_UniquePtrInsert)
{
    auto a = std::make_unique<int>(1);
    auto b = std::make_unique<int>(2);

    OpkUniqueList<int> list;
    list.insert(OpaquePtrKey<int>(a));
    list.insert(OpaquePtrKey<int>(b));

    EXPECT_EQ(list.size(), 2u);
    // Keys remain valid and findable as long as unique_ptrs exist
    EXPECT_EQ(list.find(OpaquePtrKey<int>(a)), 0u);
}

TEST(OpkUniqueListTests, SmartPointer_WeakPtrInsert)
{
    auto sp = std::make_shared<int>(42);
    std::weak_ptr<int> wp = sp;

    OpkUniqueList<int> list;
    list.insert(OpaquePtrKey<int>(wp));

    EXPECT_EQ(list.size(), 1u);
    // Can find using the shared_ptr
    EXPECT_EQ(list.find(OpaquePtrKey<int>(sp)), 0u);
}

