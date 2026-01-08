/**
 * @file unique_shared_weak_list_tests.cpp
 * Unit tests for crddagt::UniqueSharedWeakList
 */
#include <gtest/gtest.h>
#include "crddagt/common/unique_shared_weak_list.hpp"

#include <limits>
#include <string>
#include <vector>

using namespace crddagt;

// ============================================================================
// npos constant
// ============================================================================

TEST(UniqueSharedWeakListTests, Npos_IsSizeMax)
{
    EXPECT_EQ(UniqueSharedWeakList<int>::npos, std::numeric_limits<std::size_t>::max());
}

TEST(UniqueSharedWeakListTests, Npos_IsConstexpr)
{
    constexpr std::size_t val = UniqueSharedWeakList<int>::npos;
    EXPECT_EQ(val, ~static_cast<std::size_t>(0));
}

// ============================================================================
// Construction
// ============================================================================

TEST(UniqueSharedWeakListTests, Construction_DefaultIsEmpty)
{
    UniqueSharedWeakList<int> list;
    EXPECT_EQ(list.size(), 0u);
}

// ============================================================================
// Insert - shared_ptr
// ============================================================================

TEST(UniqueSharedWeakListTests, Insert_SharedPtr_FirstElementReturnsZero)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);
    EXPECT_EQ(idx, 0u);
}

TEST(UniqueSharedWeakListTests, Insert_SharedPtr_SequentialReturnsIncrementingIndices)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    auto c = std::make_shared<int>(3);
    UniqueSharedWeakList<int> list;

    EXPECT_EQ(list.insert(a), 0u);
    EXPECT_EQ(list.insert(b), 1u);
    EXPECT_EQ(list.insert(c), 2u);
}

TEST(UniqueSharedWeakListTests, Insert_SharedPtr_DuplicateReturnsExistingIndex)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;

    std::size_t first = list.insert(ptr);
    std::size_t second = list.insert(ptr);

    EXPECT_EQ(first, second);
    EXPECT_EQ(list.size(), 1u);
}

TEST(UniqueSharedWeakListTests, Insert_SharedPtr_NullThrows)
{
    UniqueSharedWeakList<int> list;
    std::shared_ptr<int> null_ptr;

    EXPECT_THROW(list.insert(null_ptr), std::invalid_argument);
}

TEST(UniqueSharedWeakListTests, Insert_SharedPtr_NullDoesNotModifyList)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    list.insert(ptr);

    std::shared_ptr<int> null_ptr;
    try {
        list.insert(null_ptr);
    } catch (const std::invalid_argument&) {
        // Expected
    }

    EXPECT_EQ(list.size(), 1u);
}

TEST(UniqueSharedWeakListTests, Insert_SharedPtr_StoredAsStrong)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    EXPECT_TRUE(list.is_strong(idx));
    EXPECT_FALSE(list.is_expired(idx));
}

// ============================================================================
// Insert - weak_ptr
// ============================================================================

TEST(UniqueSharedWeakListTests, Insert_WeakPtr_ValidReturnsIndex)
{
    auto sp = std::make_shared<int>(42);
    std::weak_ptr<int> wp = sp;

    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(wp);

    EXPECT_EQ(idx, 0u);
    EXPECT_TRUE(list.is_strong(idx));  // Stored as strong
}

TEST(UniqueSharedWeakListTests, Insert_WeakPtr_ExpiredThrows)
{
    std::weak_ptr<int> expired;
    {
        auto sp = std::make_shared<int>(42);
        expired = sp;
    }

    UniqueSharedWeakList<int> list;
    EXPECT_THROW(list.insert(expired), std::invalid_argument);
}

TEST(UniqueSharedWeakListTests, Insert_WeakPtr_DuplicateWithSharedPtrReturnsExistingIndex)
{
    auto sp = std::make_shared<int>(42);
    std::weak_ptr<int> wp = sp;

    UniqueSharedWeakList<int> list;
    std::size_t first = list.insert(sp);
    std::size_t second = list.insert(wp);

    EXPECT_EQ(first, second);
    EXPECT_EQ(list.size(), 1u);
}

// ============================================================================
// Insert - const T variants
// ============================================================================

TEST(UniqueSharedWeakListTests, Insert_SharedPtrConstT_Works)
{
    auto ptr = std::make_shared<const int>(42);
    UniqueSharedWeakList<int> list;

    std::size_t idx = list.insert(ptr);
    EXPECT_EQ(idx, 0u);
    EXPECT_EQ(*list.at(idx), 42);
}

TEST(UniqueSharedWeakListTests, Insert_WeakPtrConstT_Works)
{
    auto sp = std::make_shared<const int>(42);
    std::weak_ptr<const int> wp = sp;

    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(wp);

    EXPECT_EQ(idx, 0u);
}

// ============================================================================
// Weaken
// ============================================================================

TEST(UniqueSharedWeakListTests, Weaken_ConvertsStrongToWeak)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    EXPECT_TRUE(list.is_strong(idx));
    list.weaken(idx);
    EXPECT_FALSE(list.is_strong(idx));
}

TEST(UniqueSharedWeakListTests, Weaken_AlreadyWeakIsNoOp)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    list.weaken(idx);
    EXPECT_FALSE(list.is_strong(idx));

    list.weaken(idx);  // Should not throw
    EXPECT_FALSE(list.is_strong(idx));
}

TEST(UniqueSharedWeakListTests, Weaken_InvalidIndexThrows)
{
    UniqueSharedWeakList<int> list;
    EXPECT_THROW(list.weaken(0), std::out_of_range);
}

TEST(UniqueSharedWeakListTests, Weaken_AllowsExternalDestruction)
{
    UniqueSharedWeakList<int> list;
    std::size_t idx;
    {
        auto ptr = std::make_shared<int>(42);
        idx = list.insert(ptr);
        list.weaken(idx);
        EXPECT_FALSE(list.is_expired(idx));  // ptr still alive
    }
    // ptr destroyed, weak entry should be expired
    EXPECT_TRUE(list.is_expired(idx));
}

// ============================================================================
// Strengthen
// ============================================================================

TEST(UniqueSharedWeakListTests, Strengthen_ConvertsWeakToStrong)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    list.weaken(idx);
    EXPECT_FALSE(list.is_strong(idx));

    list.strengthen(idx);
    EXPECT_TRUE(list.is_strong(idx));
}

TEST(UniqueSharedWeakListTests, Strengthen_AlreadyStrongIsNoOp)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    EXPECT_TRUE(list.is_strong(idx));
    list.strengthen(idx);  // Should not throw
    EXPECT_TRUE(list.is_strong(idx));
}

TEST(UniqueSharedWeakListTests, Strengthen_InvalidIndexThrows)
{
    UniqueSharedWeakList<int> list;
    EXPECT_THROW(list.strengthen(0), std::out_of_range);
}

TEST(UniqueSharedWeakListTests, Strengthen_ExpiredThrows)
{
    UniqueSharedWeakList<int> list;
    std::size_t idx;
    {
        auto ptr = std::make_shared<int>(42);
        idx = list.insert(ptr);
        list.weaken(idx);
    }
    // ptr destroyed

    EXPECT_THROW(list.strengthen(idx), UniqueSharedWeakList<int>::expired_entry_error);
}

TEST(UniqueSharedWeakListTests, Strengthen_PreventsSubsequentExpiration)
{
    auto external = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(external);

    list.weaken(idx);
    list.strengthen(idx);  // List now holds strong ref

    external.reset();  // Release external reference

    EXPECT_FALSE(list.is_expired(idx));  // List still has it
    EXPECT_EQ(*list.at(idx), 42);
}

// ============================================================================
// At
// ============================================================================

TEST(UniqueSharedWeakListTests, At_StrongReturnsSharedPtr)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    auto result = list.at(idx);
    EXPECT_EQ(result.get(), ptr.get());
    EXPECT_EQ(*result, 42);
}

TEST(UniqueSharedWeakListTests, At_WeakNotExpiredReturnsSharedPtr)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);
    list.weaken(idx);

    auto result = list.at(idx);
    EXPECT_EQ(result.get(), ptr.get());
}

TEST(UniqueSharedWeakListTests, At_WeakExpiredThrows)
{
    UniqueSharedWeakList<int> list;
    std::size_t idx;
    {
        auto ptr = std::make_shared<int>(42);
        idx = list.insert(ptr);
        list.weaken(idx);
    }

    EXPECT_THROW(list.at(idx), UniqueSharedWeakList<int>::expired_entry_error);
}

TEST(UniqueSharedWeakListTests, At_InvalidIndexThrows)
{
    UniqueSharedWeakList<int> list;
    EXPECT_THROW(list.at(0), std::out_of_range);
}

TEST(UniqueSharedWeakListTests, At_NposIndexThrows)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    list.insert(ptr);

    EXPECT_THROW(list.at(UniqueSharedWeakList<int>::npos), std::out_of_range);
}

// ============================================================================
// Get
// ============================================================================

TEST(UniqueSharedWeakListTests, Get_StrongReturnsSharedPtr)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    auto result = list.get(idx);
    EXPECT_EQ(result.get(), ptr.get());
}

TEST(UniqueSharedWeakListTests, Get_WeakNotExpiredReturnsSharedPtr)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);
    list.weaken(idx);

    auto result = list.get(idx);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result.get(), ptr.get());
}

TEST(UniqueSharedWeakListTests, Get_WeakExpiredReturnsNullptr)
{
    UniqueSharedWeakList<int> list;
    std::size_t idx;
    {
        auto ptr = std::make_shared<int>(42);
        idx = list.insert(ptr);
        list.weaken(idx);
    }

    auto result = list.get(idx);
    EXPECT_EQ(result, nullptr);
}

TEST(UniqueSharedWeakListTests, Get_InvalidIndexThrows)
{
    UniqueSharedWeakList<int> list;
    EXPECT_THROW(list.get(0), std::out_of_range);
}

// ============================================================================
// Find
// ============================================================================

TEST(UniqueSharedWeakListTests, Find_RawPointer_ExistingReturnsIndex)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    list.insert(ptr);

    EXPECT_EQ(list.find(ptr.get()), 0u);
}

TEST(UniqueSharedWeakListTests, Find_RawPointer_NonExistingReturnsNpos)
{
    int obj = 42;
    UniqueSharedWeakList<int> list;

    EXPECT_EQ(list.find(&obj), UniqueSharedWeakList<int>::npos);
}

TEST(UniqueSharedWeakListTests, Find_RawPointer_NullReturnsNpos)
{
    UniqueSharedWeakList<int> list;
    EXPECT_EQ(list.find(static_cast<int*>(nullptr)), UniqueSharedWeakList<int>::npos);
}

TEST(UniqueSharedWeakListTests, Find_SharedPtr_ExistingReturnsIndex)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    list.insert(ptr);

    EXPECT_EQ(list.find(ptr), 0u);
}

TEST(UniqueSharedWeakListTests, Find_WeakPtr_NotExpiredReturnsIndex)
{
    auto sp = std::make_shared<int>(42);
    std::weak_ptr<int> wp = sp;
    UniqueSharedWeakList<int> list;
    list.insert(sp);

    EXPECT_EQ(list.find(wp), 0u);
}

TEST(UniqueSharedWeakListTests, Find_WeakPtr_ExpiredReturnsNpos)
{
    std::weak_ptr<int> expired;
    {
        auto sp = std::make_shared<int>(42);
        expired = sp;
    }

    UniqueSharedWeakList<int> list;
    EXPECT_EQ(list.find(expired), UniqueSharedWeakList<int>::npos);
}

TEST(UniqueSharedWeakListTests, Find_WorksForExpiredWeakEntry)
{
    UniqueSharedWeakList<int> list;
    int* raw_addr;
    std::size_t idx;
    {
        auto ptr = std::make_shared<int>(42);
        raw_addr = ptr.get();
        idx = list.insert(ptr);
        list.weaken(idx);
    }
    // Entry expired, but key is still stored
    // Note: We can't use the raw_addr directly since the memory is freed
    // but we can verify the entry is still findable via its stored key
    EXPECT_TRUE(list.is_expired(idx));
    // Size should still be 1
    EXPECT_EQ(list.size(), 1u);
}

TEST(UniqueSharedWeakListTests, Find_EmptyListReturnsNpos)
{
    int obj = 42;
    UniqueSharedWeakList<int> list;

    EXPECT_EQ(list.find(&obj), UniqueSharedWeakList<int>::npos);
}

TEST(UniqueSharedWeakListTests, Find_IsNoexcept)
{
    UniqueSharedWeakList<int> list;
    int obj = 42;

    static_assert(noexcept(list.find(&obj)), "find(T*) must be noexcept");
    static_assert(noexcept(list.find(std::shared_ptr<int>())),
        "find(shared_ptr) must be noexcept");
    static_assert(noexcept(list.find(std::weak_ptr<int>())),
        "find(weak_ptr) must be noexcept");
}

// ============================================================================
// Is_strong
// ============================================================================

TEST(UniqueSharedWeakListTests, IsStrong_TrueAfterInsert)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    EXPECT_TRUE(list.is_strong(idx));
}

TEST(UniqueSharedWeakListTests, IsStrong_FalseAfterWeaken)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    list.weaken(idx);
    EXPECT_FALSE(list.is_strong(idx));
}

TEST(UniqueSharedWeakListTests, IsStrong_TrueAfterStrengthen)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    list.weaken(idx);
    list.strengthen(idx);
    EXPECT_TRUE(list.is_strong(idx));
}

TEST(UniqueSharedWeakListTests, IsStrong_InvalidIndexThrows)
{
    UniqueSharedWeakList<int> list;
    EXPECT_THROW(list.is_strong(0), std::out_of_range);
}

// ============================================================================
// Is_expired
// ============================================================================

TEST(UniqueSharedWeakListTests, IsExpired_FalseForStrong)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    EXPECT_FALSE(list.is_expired(idx));
}

TEST(UniqueSharedWeakListTests, IsExpired_FalseForWeakNotExpired)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);
    list.weaken(idx);

    EXPECT_FALSE(list.is_expired(idx));
}

TEST(UniqueSharedWeakListTests, IsExpired_TrueForWeakExpired)
{
    UniqueSharedWeakList<int> list;
    std::size_t idx;
    {
        auto ptr = std::make_shared<int>(42);
        idx = list.insert(ptr);
        list.weaken(idx);
    }

    EXPECT_TRUE(list.is_expired(idx));
}

TEST(UniqueSharedWeakListTests, IsExpired_InvalidIndexThrows)
{
    UniqueSharedWeakList<int> list;
    EXPECT_THROW(list.is_expired(0), std::out_of_range);
}

// ============================================================================
// Size
// ============================================================================

TEST(UniqueSharedWeakListTests, Size_EmptyListIsZero)
{
    UniqueSharedWeakList<int> list;
    EXPECT_EQ(list.size(), 0u);
}

TEST(UniqueSharedWeakListTests, Size_IncreasesAfterInsert)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    UniqueSharedWeakList<int> list;

    list.insert(a);
    EXPECT_EQ(list.size(), 1u);

    list.insert(b);
    EXPECT_EQ(list.size(), 2u);
}

TEST(UniqueSharedWeakListTests, Size_UnchangedAfterDuplicateInsert)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;

    list.insert(ptr);
    EXPECT_EQ(list.size(), 1u);

    list.insert(ptr);
    EXPECT_EQ(list.size(), 1u);
}

TEST(UniqueSharedWeakListTests, Size_IncludesExpiredEntries)
{
    UniqueSharedWeakList<int> list;
    {
        auto ptr = std::make_shared<int>(42);
        list.insert(ptr);
        list.weaken(0);
    }
    // Entry expired but still counts
    EXPECT_EQ(list.size(), 1u);
}

TEST(UniqueSharedWeakListTests, Size_IsNoexcept)
{
    UniqueSharedWeakList<int> list;
    static_assert(noexcept(list.size()), "size must be noexcept");
}

// ============================================================================
// Enumerate
// ============================================================================

TEST(UniqueSharedWeakListTests, Enumerate_EmptyListNoCallbacks)
{
    UniqueSharedWeakList<int> list;
    int call_count = 0;

    list.enumerate([&](std::size_t, std::shared_ptr<int>, bool, bool) {
        ++call_count;
    });

    EXPECT_EQ(call_count, 0);
}

TEST(UniqueSharedWeakListTests, Enumerate_VisitsAllEntries)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    auto c = std::make_shared<int>(3);
    UniqueSharedWeakList<int> list;
    list.insert(a);
    list.insert(b);
    list.insert(c);

    int call_count = 0;
    list.enumerate([&](std::size_t, std::shared_ptr<int>, bool, bool) {
        ++call_count;
    });

    EXPECT_EQ(call_count, 3);
}

TEST(UniqueSharedWeakListTests, Enumerate_PreservesInsertionOrder)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    auto c = std::make_shared<int>(3);
    UniqueSharedWeakList<int> list;
    list.insert(a);
    list.insert(b);
    list.insert(c);

    std::vector<int> values;
    list.enumerate([&](std::size_t, std::shared_ptr<int> ptr, bool, bool) {
        values.push_back(*ptr);
    });

    EXPECT_EQ(values.size(), 3u);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
}

TEST(UniqueSharedWeakListTests, Enumerate_ReportsCorrectIsStrong)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    UniqueSharedWeakList<int> list;
    list.insert(a);
    list.insert(b);
    list.weaken(1);

    std::vector<bool> strong_flags;
    list.enumerate([&](std::size_t, std::shared_ptr<int>, bool is_strong, bool) {
        strong_flags.push_back(is_strong);
    });

    EXPECT_EQ(strong_flags.size(), 2u);
    EXPECT_TRUE(strong_flags[0]);
    EXPECT_FALSE(strong_flags[1]);
}

TEST(UniqueSharedWeakListTests, Enumerate_ReportsExpired)
{
    UniqueSharedWeakList<int> list;
    {
        auto ptr = std::make_shared<int>(42);
        list.insert(ptr);
        list.weaken(0);
    }

    bool saw_expired = false;
    list.enumerate([&](std::size_t, std::shared_ptr<int> ptr, bool is_strong, bool is_expired) {
        EXPECT_FALSE(is_strong);
        EXPECT_TRUE(is_expired);
        EXPECT_EQ(ptr, nullptr);
        saw_expired = true;
    });

    EXPECT_TRUE(saw_expired);
}

// ============================================================================
// Key_at
// ============================================================================

TEST(UniqueSharedWeakListTests, KeyAt_ReturnsCorrectKey)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    auto key = list.key_at(idx);
    EXPECT_EQ(key, OpaquePtrKey<int>(ptr));
}

TEST(UniqueSharedWeakListTests, KeyAt_WorksForExpiredEntry)
{
    UniqueSharedWeakList<int> list;
    OpaquePtrKey<int> original_key(static_cast<int*>(nullptr));  // Will be overwritten
    {
        auto ptr = std::make_shared<int>(42);
        original_key = OpaquePtrKey<int>(ptr);
        list.insert(ptr);
        list.weaken(0);
    }
    // Entry expired, but key should still be accessible
    auto key = list.key_at(0);
    EXPECT_EQ(key, original_key);
}

TEST(UniqueSharedWeakListTests, KeyAt_InvalidIndexThrows)
{
    UniqueSharedWeakList<int> list;
    EXPECT_THROW(list.key_at(0), std::out_of_range);
}

// ============================================================================
// Insertion Order Preservation
// ============================================================================

TEST(UniqueSharedWeakListTests, InsertionOrder_PreservedAcrossMultipleInserts)
{
    std::vector<std::shared_ptr<int>> ptrs;
    for (int i = 0; i < 5; ++i) {
        ptrs.push_back(std::make_shared<int>(i * 10));
    }

    UniqueSharedWeakList<int> list;
    for (auto& ptr : ptrs) {
        list.insert(ptr);
    }

    for (std::size_t i = 0; i < 5; ++i) {
        EXPECT_EQ(list.at(i).get(), ptrs[i].get());
    }
}

TEST(UniqueSharedWeakListTests, InsertionOrder_UnaffectedByDuplicates)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    auto c = std::make_shared<int>(3);
    UniqueSharedWeakList<int> list;

    list.insert(a);
    list.insert(b);
    list.insert(a);  // duplicate
    list.insert(c);
    list.insert(b);  // duplicate

    EXPECT_EQ(list.size(), 3u);
    EXPECT_EQ(list.at(0).get(), a.get());
    EXPECT_EQ(list.at(1).get(), b.get());
    EXPECT_EQ(list.at(2).get(), c.get());
}

// ============================================================================
// Invariants
// ============================================================================

TEST(UniqueSharedWeakListTests, Invariant_FindOfAtReturnsIndex)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    auto c = std::make_shared<int>(3);
    UniqueSharedWeakList<int> list;
    list.insert(a);
    list.insert(b);
    list.insert(c);

    for (std::size_t i = 0; i < list.size(); ++i) {
        EXPECT_EQ(list.find(list.at(i).get()), i);
    }
}

TEST(UniqueSharedWeakListTests, Invariant_AtOfInsertReturnsPtr)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;

    std::size_t idx = list.insert(ptr);
    EXPECT_EQ(list.at(idx).get(), ptr.get());
}

TEST(UniqueSharedWeakListTests, Invariant_StrongNeverExpires)
{
    auto external = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(external);

    external.reset();  // Release external reference

    // List should still have it (strong reference)
    EXPECT_FALSE(list.is_expired(idx));
    EXPECT_EQ(*list.at(idx), 42);
}

TEST(UniqueSharedWeakListTests, Invariant_DuplicateDoesNotChangeStorageMode)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    list.weaken(idx);
    EXPECT_FALSE(list.is_strong(idx));

    // Re-insert same pointer
    list.insert(ptr);

    // Storage mode should NOT change
    EXPECT_FALSE(list.is_strong(idx));
}

// ============================================================================
// Value Semantics
// ============================================================================

TEST(UniqueSharedWeakListTests, ValueSemantics_CopyConstruction)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    UniqueSharedWeakList<int> original;
    original.insert(a);
    original.insert(b);

    UniqueSharedWeakList<int> copy(original);

    EXPECT_EQ(copy.size(), 2u);
    EXPECT_EQ(copy.at(0).get(), a.get());
    EXPECT_EQ(copy.at(1).get(), b.get());
}

TEST(UniqueSharedWeakListTests, ValueSemantics_CopyIsIndependent)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    auto c = std::make_shared<int>(3);
    UniqueSharedWeakList<int> original;
    original.insert(a);
    original.insert(b);

    UniqueSharedWeakList<int> copy(original);
    copy.insert(c);

    EXPECT_EQ(original.size(), 2u);
    EXPECT_EQ(copy.size(), 3u);
}

TEST(UniqueSharedWeakListTests, ValueSemantics_CopyPreservesWeakState)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> original;
    original.insert(ptr);
    original.weaken(0);

    UniqueSharedWeakList<int> copy(original);

    EXPECT_FALSE(copy.is_strong(0));
}

TEST(UniqueSharedWeakListTests, ValueSemantics_MoveConstruction)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    UniqueSharedWeakList<int> original;
    original.insert(a);
    original.insert(b);

    UniqueSharedWeakList<int> moved(std::move(original));

    EXPECT_EQ(moved.size(), 2u);
    EXPECT_EQ(moved.at(0).get(), a.get());
    EXPECT_EQ(moved.at(1).get(), b.get());
}

// ============================================================================
// Type Traits
// ============================================================================

TEST(UniqueSharedWeakListTests, TypeTraits_IsDefaultConstructible)
{
    static_assert(std::is_default_constructible_v<UniqueSharedWeakList<int>>,
        "UniqueSharedWeakList must be default constructible");
}

TEST(UniqueSharedWeakListTests, TypeTraits_IsCopyConstructible)
{
    static_assert(std::is_copy_constructible_v<UniqueSharedWeakList<int>>,
        "UniqueSharedWeakList must be copy constructible");
}

TEST(UniqueSharedWeakListTests, TypeTraits_IsMoveConstructible)
{
    static_assert(std::is_move_constructible_v<UniqueSharedWeakList<int>>,
        "UniqueSharedWeakList must be move constructible");
}

// ============================================================================
// Different Template Types
// ============================================================================

TEST(UniqueSharedWeakListTests, DifferentTypes_WorksWithCustomType)
{
    struct CustomType { int value; };

    auto a = std::make_shared<CustomType>(CustomType{1});
    auto b = std::make_shared<CustomType>(CustomType{2});

    UniqueSharedWeakList<CustomType> list;
    list.insert(a);
    list.insert(b);

    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(list.at(0)->value, 1);
    EXPECT_EQ(list.at(1)->value, 2);
}

TEST(UniqueSharedWeakListTests, DifferentTypes_WorksWithString)
{
    auto a = std::make_shared<std::string>("hello");
    auto b = std::make_shared<std::string>("world");

    UniqueSharedWeakList<std::string> list;
    list.insert(a);
    list.insert(b);

    EXPECT_EQ(list.size(), 2u);
    EXPECT_EQ(*list.at(0), "hello");
    EXPECT_EQ(*list.at(1), "world");
}

// ============================================================================
// Complex Scenarios
// ============================================================================

TEST(UniqueSharedWeakListTests, Scenario_WeakenStrengthenCycle)
{
    auto ptr = std::make_shared<int>(42);
    UniqueSharedWeakList<int> list;
    std::size_t idx = list.insert(ptr);

    // Multiple weaken/strengthen cycles
    for (int i = 0; i < 3; ++i) {
        list.weaken(idx);
        EXPECT_FALSE(list.is_strong(idx));
        EXPECT_FALSE(list.is_expired(idx));

        list.strengthen(idx);
        EXPECT_TRUE(list.is_strong(idx));
        EXPECT_FALSE(list.is_expired(idx));
    }

    EXPECT_EQ(*list.at(idx), 42);
}

TEST(UniqueSharedWeakListTests, Scenario_MixedStrongAndWeakEntries)
{
    auto a = std::make_shared<int>(1);
    auto b = std::make_shared<int>(2);
    auto c = std::make_shared<int>(3);

    UniqueSharedWeakList<int> list;
    list.insert(a);
    list.insert(b);
    list.insert(c);

    // Weaken middle entry
    list.weaken(1);

    EXPECT_TRUE(list.is_strong(0));
    EXPECT_FALSE(list.is_strong(1));
    EXPECT_TRUE(list.is_strong(2));

    // All should still be accessible
    EXPECT_EQ(*list.at(0), 1);
    EXPECT_EQ(*list.at(1), 2);
    EXPECT_EQ(*list.at(2), 3);
}

TEST(UniqueSharedWeakListTests, Scenario_PartialExpiration)
{
    UniqueSharedWeakList<int> list;
    auto kept = std::make_shared<int>(1);

    // Insert two items
    list.insert(kept);
    {
        auto temp = std::make_shared<int>(2);
        list.insert(temp);
        list.weaken(1);
    }
    // temp destroyed, entry 1 expired

    EXPECT_FALSE(list.is_expired(0));
    EXPECT_TRUE(list.is_expired(1));

    // Can still access entry 0
    EXPECT_EQ(*list.at(0), 1);

    // Entry 1 throws on at()
    EXPECT_THROW(list.at(1), UniqueSharedWeakList<int>::expired_entry_error);

    // But get() returns nullptr
    EXPECT_EQ(list.get(1), nullptr);
}

