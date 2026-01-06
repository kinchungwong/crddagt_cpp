#include <gtest/gtest.h>
#include "crddagt/common/opaque_ptr_key.hpp"

using namespace crddagt;

TEST(OpaquePtrKeyTests, BasicFunctionality)
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

TEST(OpaquePtrKeyTests, NoOperatorsDefinedForDifferentTypes)
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

TEST(OpaquePtrKeyTests, SmartPointerConstruction)
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
