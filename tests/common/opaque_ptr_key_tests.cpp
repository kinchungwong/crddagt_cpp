#include <gtest/gtest.h>
#include "crddagt/common/opaque_ptr_key.hpp"

#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

using namespace crddagt;

TEST(OpaquePtrKeyTests, Smoke_BasicFunctionality)
{
    int a = 42;
    int b = 42;
    int c = 100;

    OpaquePtrKey<int> opk_a1(&a);
    OpaquePtrKey<int> opk_a2(&a);
    OpaquePtrKey<int> opk_b(&b);
    OpaquePtrKey<int> opk_c(&c);

    // Test equality
    EXPECT_EQ(opk_a1, opk_a2);
    EXPECT_NE(opk_a1, opk_b);
    EXPECT_NE(opk_a1, opk_c);

    // Test ordering
    EXPECT_FALSE(opk_a1 > opk_a2);
    EXPECT_TRUE(opk_c > opk_a1 || opk_a1 > opk_c); // One must be greater

    // Test hash functionality
    std::hash<OpaquePtrKey<int>> hasher;
    EXPECT_EQ(hasher(opk_a1), hasher(opk_a2));
    EXPECT_NE(hasher(opk_a1), hasher(opk_b));
}

TEST(OpaquePtrKeyTests, Smoke_TypeSafety)
{
    std::string s = "test";
    const char* s0 = s.c_str();
    OpaquePtrKey<std::string> opk_str_s(&s);
    OpaquePtrKey<char> opk_pchar_s0(s0);

    bool has_cross_eq = std::is_invocable_v<
        decltype(&OpaquePtrKey<std::string>::operator==), 
        OpaquePtrKey<std::string>, 
        OpaquePtrKey<char>
    >;
    EXPECT_FALSE(has_cross_eq);
    bool has_cross_ne = std::is_invocable_v<
        decltype(&OpaquePtrKey<std::string>::operator!=), 
        OpaquePtrKey<std::string>, 
        OpaquePtrKey<char>
    >;
    EXPECT_FALSE(has_cross_ne);
    bool has_cross_assign = std::is_assignable_v<
        OpaquePtrKey<std::string>&, 
        OpaquePtrKey<char>
    >;
    EXPECT_FALSE(has_cross_assign);
}

TEST(OpaquePtrKeyTests, Smoke_SmartPointerConstruction)
{
    auto sptr1 = std::make_shared<int>(10);
    auto sptr2 = std::make_shared<int>(10);
    auto uptr1 = std::make_unique<int>(20);
    auto uptr2 = std::make_unique<int>(20);

    OpaquePtrKey<int> opk_sptr1(sptr1);
    OpaquePtrKey<int> opk_sptr2(sptr2);
    OpaquePtrKey<int> opk_uptr1(uptr1);
    OpaquePtrKey<int> opk_uptr2(uptr2);

    // Different shared_ptrs to same value should be different keys
    EXPECT_NE(opk_sptr1, opk_sptr2);

    // Different unique_ptrs to same value should be different keys
    EXPECT_NE(opk_uptr1, opk_uptr2);

    // Cross comparison should also be different
    EXPECT_NE(opk_sptr1, opk_uptr1);

    // Same shared_ptr should yield same key
    auto sptr1_copy = sptr1;
    OpaquePtrKey<int> opk_sptr1_copy(sptr1_copy);
    EXPECT_EQ(opk_sptr1, opk_sptr1_copy);

    // Same unique_ptr should yield same key
    auto uptr1_move = std::move(uptr1);
    OpaquePtrKey<int> opk_uptr1_move(uptr1_move);
    EXPECT_EQ(opk_uptr1, opk_uptr1_move);

    // Still pointing to same object after converting unique_ptr to shared_ptr
    auto shared_from_uptr2 = std::shared_ptr<int>(std::move(uptr2));
    OpaquePtrKey<int> opk_shared_from_uptr2(shared_from_uptr2);
    EXPECT_EQ(opk_uptr2, opk_shared_from_uptr2);

    // Test null-initialized weak_ptr
    std::weak_ptr<int> wptr_empty;
    OpaquePtrKey<int> opk_wptr_empty(wptr_empty);
    EXPECT_TRUE(!opk_wptr_empty);

    // Test weak_ptr construction
    std::weak_ptr<int> wptr2 = sptr2;
    OpaquePtrKey<int> opk_wptr2(wptr2);
    EXPECT_EQ(opk_sptr2, opk_wptr2);

    // Test expiring the shared_ptr that backs a weak_ptr and then
    // constructing OpaquePtrKey from the expired weak_ptr
    sptr2 = nullptr;
    OpaquePtrKey<int> opk_wptr2_expired(wptr2);
    EXPECT_TRUE(!opk_wptr2_expired);

    // However, OpaquePtrKey that was created from before expiration
    // should still be valid
    EXPECT_EQ(opk_sptr2, opk_wptr2);
    EXPECT_NE(opk_sptr2, opk_wptr2_expired);
}

// ============================================================================
// Independent test cases (one aspect per test)
// ============================================================================

// ----------------------------------------------------------------------------
// Null state tests
// ----------------------------------------------------------------------------

TEST(OpaquePtrKeyTests, NullState_FromNullptr)
{
    OpaquePtrKey<int> opk(static_cast<int*>(nullptr));
    EXPECT_TRUE(!opk);
}

TEST(OpaquePtrKeyTests, NullState_OperatorNotOnNonNull)
{
    int x = 42;
    OpaquePtrKey<int> opk(&x);
    EXPECT_FALSE(!opk);
}

TEST(OpaquePtrKeyTests, NullState_TwoNullKeysAreEqual)
{
    OpaquePtrKey<int> opk1(static_cast<int*>(nullptr));
    OpaquePtrKey<int> opk2(static_cast<int*>(nullptr));
    EXPECT_EQ(opk1, opk2);
}

TEST(OpaquePtrKeyTests, NullState_NullNotEqualToNonNull)
{
    int x = 42;
    OpaquePtrKey<int> opk_null(static_cast<int*>(nullptr));
    OpaquePtrKey<int> opk_valid(&x);
    EXPECT_NE(opk_null, opk_valid);
}

// ----------------------------------------------------------------------------
// Equality operator tests
// ----------------------------------------------------------------------------

TEST(OpaquePtrKeyTests, Equality_SamePointerIsEqual)
{
    int x = 42;
    OpaquePtrKey<int> opk1(&x);
    OpaquePtrKey<int> opk2(&x);
    EXPECT_TRUE(opk1 == opk2);
    EXPECT_FALSE(opk1 != opk2);
}

TEST(OpaquePtrKeyTests, Equality_DifferentPointersNotEqual)
{
    int x = 42;
    int y = 42; // Same value, different address
    OpaquePtrKey<int> opk_x(&x);
    OpaquePtrKey<int> opk_y(&y);
    EXPECT_FALSE(opk_x == opk_y);
    EXPECT_TRUE(opk_x != opk_y);
}

TEST(OpaquePtrKeyTests, Equality_SelfComparison)
{
    int x = 42;
    OpaquePtrKey<int> opk(&x);
    EXPECT_TRUE(opk == opk);
    EXPECT_FALSE(opk != opk);
}

// ----------------------------------------------------------------------------
// Ordering operator tests
// ----------------------------------------------------------------------------

TEST(OpaquePtrKeyTests, Ordering_LessThan)
{
    int arr[2] = {1, 2};
    OpaquePtrKey<int> opk0(&arr[0]);
    OpaquePtrKey<int> opk1(&arr[1]);
    // Array elements are contiguous; &arr[0] < &arr[1]
    EXPECT_TRUE(opk0 < opk1);
    EXPECT_FALSE(opk1 < opk0);
}

TEST(OpaquePtrKeyTests, Ordering_GreaterThan)
{
    int arr[2] = {1, 2};
    OpaquePtrKey<int> opk0(&arr[0]);
    OpaquePtrKey<int> opk1(&arr[1]);
    EXPECT_TRUE(opk1 > opk0);
    EXPECT_FALSE(opk0 > opk1);
}

TEST(OpaquePtrKeyTests, Ordering_LessThanOrEqual)
{
    int arr[2] = {1, 2};
    OpaquePtrKey<int> opk0(&arr[0]);
    OpaquePtrKey<int> opk0_copy(&arr[0]);
    OpaquePtrKey<int> opk1(&arr[1]);
    EXPECT_TRUE(opk0 <= opk1);
    EXPECT_TRUE(opk0 <= opk0_copy);
    EXPECT_FALSE(opk1 <= opk0);
}

TEST(OpaquePtrKeyTests, Ordering_GreaterThanOrEqual)
{
    int arr[2] = {1, 2};
    OpaquePtrKey<int> opk0(&arr[0]);
    OpaquePtrKey<int> opk1(&arr[1]);
    OpaquePtrKey<int> opk1_copy(&arr[1]);
    EXPECT_TRUE(opk1 >= opk0);
    EXPECT_TRUE(opk1 >= opk1_copy);
    EXPECT_FALSE(opk0 >= opk1);
}

TEST(OpaquePtrKeyTests, Ordering_Reflexivity)
{
    int x = 42;
    OpaquePtrKey<int> opk(&x);
    EXPECT_FALSE(opk < opk);
    EXPECT_FALSE(opk > opk);
    EXPECT_TRUE(opk <= opk);
    EXPECT_TRUE(opk >= opk);
}

TEST(OpaquePtrKeyTests, Ordering_Antisymmetry)
{
    int arr[2] = {1, 2};
    OpaquePtrKey<int> opk0(&arr[0]);
    OpaquePtrKey<int> opk1(&arr[1]);
    // If a < b, then !(b < a)
    if (opk0 < opk1)
    {
        EXPECT_FALSE(opk1 < opk0);
    }
    else
    {
        EXPECT_FALSE(opk0 < opk1);
    }
}

TEST(OpaquePtrKeyTests, Ordering_Transitivity)
{
    int arr[3] = {1, 2, 3};
    OpaquePtrKey<int> opk0(&arr[0]);
    OpaquePtrKey<int> opk1(&arr[1]);
    OpaquePtrKey<int> opk2(&arr[2]);
    // arr[0] < arr[1] < arr[2] due to contiguous layout
    EXPECT_TRUE(opk0 < opk1);
    EXPECT_TRUE(opk1 < opk2);
    EXPECT_TRUE(opk0 < opk2); // Transitivity
}

// ----------------------------------------------------------------------------
// Construction from const T smart pointers
// ----------------------------------------------------------------------------

TEST(OpaquePtrKeyTests, ConstSmartPtr_UniquePtr)
{
    auto uptr = std::make_unique<const int>(42);
    const int* raw = uptr.get();
    OpaquePtrKey<int> opk_from_uptr(uptr);
    OpaquePtrKey<int> opk_from_raw(raw);
    EXPECT_EQ(opk_from_uptr, opk_from_raw);
    EXPECT_FALSE(!opk_from_uptr);
}

TEST(OpaquePtrKeyTests, ConstSmartPtr_SharedPtr)
{
    auto sptr = std::make_shared<const int>(42);
    const int* raw = sptr.get();
    OpaquePtrKey<int> opk_from_sptr(sptr);
    OpaquePtrKey<int> opk_from_raw(raw);
    EXPECT_EQ(opk_from_sptr, opk_from_raw);
    EXPECT_FALSE(!opk_from_sptr);
}

TEST(OpaquePtrKeyTests, ConstSmartPtr_WeakPtr)
{
    auto sptr = std::make_shared<const int>(42);
    std::weak_ptr<const int> wptr = sptr;
    OpaquePtrKey<int> opk_from_sptr(sptr);
    OpaquePtrKey<int> opk_from_wptr(wptr);
    EXPECT_EQ(opk_from_sptr, opk_from_wptr);
    EXPECT_FALSE(!opk_from_wptr);
}

// ----------------------------------------------------------------------------
// Type-dependent hashing tests
// ----------------------------------------------------------------------------

TEST(OpaquePtrKeyTests, Hash_SameKeyHashesConsistently)
{
    int x = 42;
    OpaquePtrKey<int> opk(&x);
    std::size_t hash1 = std::hash<OpaquePtrKey<int>>{}(opk);
    std::size_t hash2 = std::hash<OpaquePtrKey<int>>{}(opk);
    std::size_t hash3 = opk.hash();
    EXPECT_EQ(hash1, hash2);
    EXPECT_EQ(hash1, hash3);
}

TEST(OpaquePtrKeyTests, Hash_EqualKeysHaveEqualHashes)
{
    int x = 42;
    OpaquePtrKey<int> opk1(&x);
    OpaquePtrKey<int> opk2(&x);
    EXPECT_EQ(opk1.hash(), opk2.hash());
}

TEST(OpaquePtrKeyTests, Hash_DifferentTypesHaveDifferentHashes)
{
    // Use a union to get the same address for different types
    union
    {
        int i;
        float f;
    } u;
    u.i = 42;

    OpaquePtrKey<int> opk_int(&u.i);
    OpaquePtrKey<float> opk_float(&u.f);

    // Same address, but different T → different hash
    EXPECT_NE(opk_int.hash(), opk_float.hash());
}

TEST(OpaquePtrKeyTests, Hash_NullKeysOfSameTypeHaveEqualHashes)
{
    OpaquePtrKey<int> opk1(static_cast<int*>(nullptr));
    OpaquePtrKey<int> opk2(static_cast<int*>(nullptr));
    EXPECT_EQ(opk1.hash(), opk2.hash());
}

// ----------------------------------------------------------------------------
// Value semantics tests
// ----------------------------------------------------------------------------

TEST(OpaquePtrKeyTests, ValueSemantics_CopyConstruction)
{
    int x = 42;
    OpaquePtrKey<int> opk1(&x);
    OpaquePtrKey<int> opk2(opk1); // Copy construct
    EXPECT_EQ(opk1, opk2);
    EXPECT_EQ(opk1.hash(), opk2.hash());
}

TEST(OpaquePtrKeyTests, ValueSemantics_CopyAssignment)
{
    int x = 42;
    int y = 100;
    OpaquePtrKey<int> opk1(&x);
    OpaquePtrKey<int> opk2(&y);
    EXPECT_NE(opk1, opk2);
    opk2 = opk1; // Copy assign
    EXPECT_EQ(opk1, opk2);
}

TEST(OpaquePtrKeyTests, ValueSemantics_MoveConstruction)
{
    int x = 42;
    OpaquePtrKey<int> opk1(&x);
    auto hash_before = opk1.hash();
    OpaquePtrKey<int> opk2(std::move(opk1)); // Move construct
    EXPECT_EQ(opk2.hash(), hash_before);
}

TEST(OpaquePtrKeyTests, ValueSemantics_MoveAssignment)
{
    int x = 42;
    int y = 100;
    OpaquePtrKey<int> opk1(&x);
    OpaquePtrKey<int> opk2(&y);
    auto hash_x = opk1.hash();
    opk2 = std::move(opk1); // Move assign
    EXPECT_EQ(opk2.hash(), hash_x);
}

// ----------------------------------------------------------------------------
// Type traits tests
// ----------------------------------------------------------------------------

TEST(OpaquePtrKeyTests, TypeTraits_TriviallyCopyable)
{
    EXPECT_TRUE(std::is_trivially_copyable_v<OpaquePtrKey<int>>);
    EXPECT_TRUE(std::is_trivially_copyable_v<OpaquePtrKey<std::string>>);
}

TEST(OpaquePtrKeyTests, TypeTraits_NoDefaultConstructor)
{
    EXPECT_FALSE(std::is_default_constructible_v<OpaquePtrKey<int>>);
    EXPECT_FALSE(std::is_default_constructible_v<OpaquePtrKey<std::string>>);
}

TEST(OpaquePtrKeyTests, TypeTraits_NothrowOperations)
{
    EXPECT_TRUE(std::is_nothrow_copy_constructible_v<OpaquePtrKey<int>>);
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<OpaquePtrKey<int>>);
    EXPECT_TRUE(std::is_nothrow_copy_assignable_v<OpaquePtrKey<int>>);
    EXPECT_TRUE(std::is_nothrow_move_assignable_v<OpaquePtrKey<int>>);
}

TEST(OpaquePtrKeyTests, TypeTraits_NoCrossTypeComparison)
{
    // Verify at compile time that cross-type comparison is not possible
    constexpr bool can_compare_eq = std::is_invocable_v<
        std::equal_to<>,
        OpaquePtrKey<int>,
        OpaquePtrKey<float>>;
    // std::equal_to<> would work if operator== existed; it doesn't for cross-type
    // But the actual member operator== won't accept different types
    constexpr bool has_member_eq = std::is_invocable_v<
        decltype(&OpaquePtrKey<int>::operator==),
        OpaquePtrKey<int>,
        OpaquePtrKey<float>>;
    EXPECT_FALSE(has_member_eq);
}

// ----------------------------------------------------------------------------
// Container usage tests
// ----------------------------------------------------------------------------

TEST(OpaquePtrKeyTests, Container_UnorderedSet)
{
    int a = 1, b = 2, c = 3;
    std::unordered_set<OpaquePtrKey<int>> set;

    set.insert(OpaquePtrKey<int>(&a));
    set.insert(OpaquePtrKey<int>(&b));
    set.insert(OpaquePtrKey<int>(&a)); // Duplicate

    EXPECT_EQ(set.size(), 2u);
    EXPECT_TRUE(set.count(OpaquePtrKey<int>(&a)) == 1);
    EXPECT_TRUE(set.count(OpaquePtrKey<int>(&b)) == 1);
    EXPECT_TRUE(set.count(OpaquePtrKey<int>(&c)) == 0);
}

TEST(OpaquePtrKeyTests, Container_UnorderedMap)
{
    int a = 1, b = 2;
    std::unordered_map<OpaquePtrKey<int>, std::string> map;

    map[OpaquePtrKey<int>(&a)] = "alpha";
    map[OpaquePtrKey<int>(&b)] = "beta";

    EXPECT_EQ(map.size(), 2u);
    EXPECT_EQ(map[OpaquePtrKey<int>(&a)], "alpha");
    EXPECT_EQ(map[OpaquePtrKey<int>(&b)], "beta");
}

TEST(OpaquePtrKeyTests, Container_Set)
{
    int arr[3] = {1, 2, 3};
    std::set<OpaquePtrKey<int>> set;

    set.insert(OpaquePtrKey<int>(&arr[2]));
    set.insert(OpaquePtrKey<int>(&arr[0]));
    set.insert(OpaquePtrKey<int>(&arr[1]));
    set.insert(OpaquePtrKey<int>(&arr[0])); // Duplicate

    EXPECT_EQ(set.size(), 3u);

    // Verify ordering: arr[0] < arr[1] < arr[2]
    auto it = set.begin();
    EXPECT_EQ(*it, OpaquePtrKey<int>(&arr[0]));
    ++it;
    EXPECT_EQ(*it, OpaquePtrKey<int>(&arr[1]));
    ++it;
    EXPECT_EQ(*it, OpaquePtrKey<int>(&arr[2]));
}

TEST(OpaquePtrKeyTests, Container_Map)
{
    int arr[2] = {1, 2};
    std::map<OpaquePtrKey<int>, std::string> map;

    map[OpaquePtrKey<int>(&arr[1])] = "second";
    map[OpaquePtrKey<int>(&arr[0])] = "first";

    EXPECT_EQ(map.size(), 2u);

    // Verify ordering
    auto it = map.begin();
    EXPECT_EQ(it->second, "first");
    ++it;
    EXPECT_EQ(it->second, "second");
}
