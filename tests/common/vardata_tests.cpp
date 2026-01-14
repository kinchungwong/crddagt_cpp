#include <gtest/gtest.h>
#include "crddagt/common/vardata.hpp"
#include "crddagt/common/vardata.inline.hpp"
#include <string>

using namespace crddagt;

// =============================================================================
// VarData Basic Functionality Tests
// =============================================================================

class VarDataTests : public ::testing::Test
{
protected:
    VarData var;
};

TEST_F(VarDataTests, DefaultConstructed_IsEmpty)
{
    EXPECT_FALSE(var.has_value());
    EXPECT_EQ(var.type(), std::type_index{typeid(void)});
}

TEST_F(VarDataTests, Set_MakesHasValueTrue)
{
    var.set(42);
    EXPECT_TRUE(var.has_value());
}

TEST_F(VarDataTests, Set_SetsCorrectType)
{
    var.set(42);
    EXPECT_EQ(var.type(), std::type_index{typeid(int)});
}

TEST_F(VarDataTests, Set_HasTypeReturnsTrue)
{
    var.set(42);
    EXPECT_TRUE(var.has_type<int>());
}

TEST_F(VarDataTests, Set_HasTypeReturnsFalseForWrongType)
{
    var.set(42);
    EXPECT_FALSE(var.has_type<double>());
    EXPECT_FALSE(var.has_type<std::string>());
}

TEST_F(VarDataTests, As_ReturnsCorrectValue)
{
    var.set(42);
    EXPECT_EQ(var.as<int>(), 42);
}

TEST_F(VarDataTests, As_CanModifyValue)
{
    var.set(42);
    var.as<int>() = 100;
    EXPECT_EQ(var.as<int>(), 100);
}

TEST_F(VarDataTests, AsConst_ReturnsCorrectValue)
{
    var.set(42);
    const VarData& cvar = var;
    EXPECT_EQ(cvar.as<int>(), 42);
}

TEST_F(VarDataTests, TryAs_ReturnsPointerOnMatch)
{
    var.set(42);
    int* ptr = var.try_as<int>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 42);
}

TEST_F(VarDataTests, TryAs_ReturnsNullptrOnMismatch)
{
    var.set(42);
    double* ptr = var.try_as<double>();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(VarDataTests, TryAs_ReturnsNullptrOnEmpty)
{
    int* ptr = var.try_as<int>();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(VarDataTests, Get_ReturnsSharedPtrOnMatch)
{
    var.set(42);
    auto ptr = var.get<int>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 42);
}

TEST_F(VarDataTests, Get_ReturnsNullptrOnMismatch)
{
    var.set(42);
    auto ptr = var.get<double>();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(VarDataTests, Get_ReturnsNullptrOnEmpty)
{
    auto ptr = var.get<int>();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(VarDataTests, Get_SharesOwnership)
{
    var.set(42);
    auto ptr1 = var.get<int>();
    auto ptr2 = var.get<int>();
    EXPECT_EQ(ptr1, ptr2);
    EXPECT_EQ(ptr1.use_count(), 3);  // var + ptr1 + ptr2
}

TEST_F(VarDataTests, Release_ReturnsValueAndResetsOnMatch)
{
    var.set(42);
    auto ptr = var.release<int>();
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 42);
    EXPECT_FALSE(var.has_value());
}

TEST_F(VarDataTests, Release_ReturnsNullptrOnMismatch)
{
    var.set(42);
    auto ptr = var.release<double>();
    EXPECT_EQ(ptr, nullptr);
    EXPECT_TRUE(var.has_value());  // Not reset on mismatch
}

TEST_F(VarDataTests, Release_ReturnsNullptrOnEmpty)
{
    auto ptr = var.release<int>();
    EXPECT_EQ(ptr, nullptr);
}

TEST_F(VarDataTests, Reset_ClearsValue)
{
    var.set(42);
    var.reset();
    EXPECT_FALSE(var.has_value());
    EXPECT_EQ(var.type(), std::type_index{typeid(void)});
}

TEST_F(VarDataTests, Emplace_ConstructsInPlace)
{
    var.emplace<std::string>("hello");
    EXPECT_TRUE(var.has_value());
    EXPECT_TRUE(var.has_type<std::string>());
    EXPECT_EQ(var.as<std::string>(), "hello");
}

TEST_F(VarDataTests, Emplace_WithMultipleArgs)
{
    var.emplace<std::string>(5, 'x');
    EXPECT_EQ(var.as<std::string>(), "xxxxx");
}

// =============================================================================
// VarData Exception Tests
// =============================================================================

TEST_F(VarDataTests, As_ThrowsOnEmpty)
{
    EXPECT_THROW(var.as<int>(), VarDataEmptyError);
}

TEST_F(VarDataTests, As_ThrowsOnTypeMismatch)
{
    var.set(42);
    EXPECT_THROW(var.as<double>(), VarDataTypeError);
}

TEST_F(VarDataTests, AsConst_ThrowsOnEmpty)
{
    const VarData& cvar = var;
    EXPECT_THROW(cvar.as<int>(), VarDataEmptyError);
}

TEST_F(VarDataTests, AsConst_ThrowsOnTypeMismatch)
{
    var.set(42);
    const VarData& cvar = var;
    EXPECT_THROW(cvar.as<double>(), VarDataTypeError);
}

// =============================================================================
// VarData Type Handling Tests
// =============================================================================

TEST_F(VarDataTests, WorksWithString)
{
    var.set(std::string("hello world"));
    EXPECT_TRUE(var.has_type<std::string>());
    EXPECT_EQ(var.as<std::string>(), "hello world");
}

TEST_F(VarDataTests, WorksWithVector)
{
    var.set(std::vector<int>{1, 2, 3});
    EXPECT_TRUE(var.has_type<std::vector<int>>());
    auto& vec = var.as<std::vector<int>>();
    EXPECT_EQ(vec.size(), 3u);
    EXPECT_EQ(vec[0], 1);
}

struct CustomType
{
    int x;
    std::string y;
    bool operator==(const CustomType& other) const
    {
        return x == other.x && y == other.y;
    }
};

TEST_F(VarDataTests, WorksWithCustomType)
{
    CustomType ct{42, "test"};
    var.set(ct);
    EXPECT_TRUE(var.has_type<CustomType>());
    EXPECT_EQ(var.as<CustomType>(), ct);
}

TEST_F(VarDataTests, MoveSemantics_MovesValue)
{
    auto str = std::make_unique<std::string>("hello");
    std::string* addr = str.get();
    var.set(std::move(str));
    EXPECT_TRUE(var.has_type<std::unique_ptr<std::string>>());
    auto& stored = var.as<std::unique_ptr<std::string>>();
    EXPECT_EQ(stored.get(), addr);
}

// =============================================================================
// VarData Copy Semantics Tests
// =============================================================================

TEST_F(VarDataTests, CopyConstruction_SharesData)
{
    var.set(42);
    VarData var2 = var;
    EXPECT_TRUE(var.has_value());
    EXPECT_TRUE(var2.has_value());
    EXPECT_EQ(var.as<int>(), var2.as<int>());
}

TEST_F(VarDataTests, CopyAssignment_SharesData)
{
    var.set(42);
    VarData var2;
    var2 = var;
    EXPECT_TRUE(var.has_value());
    EXPECT_TRUE(var2.has_value());
    EXPECT_EQ(var.as<int>(), var2.as<int>());
}

TEST_F(VarDataTests, CopyModification_AffectsBoth)
{
    var.set(42);
    VarData var2 = var;
    var.as<int>() = 100;
    EXPECT_EQ(var2.as<int>(), 100);  // Shared data
}

TEST_F(VarDataTests, MoveConstruction_TransfersOwnership)
{
    var.set(42);
    VarData var2 = std::move(var);
    EXPECT_TRUE(var2.has_value());
    EXPECT_EQ(var2.as<int>(), 42);
}

// =============================================================================
// VarData Type Decay Tests
// =============================================================================

TEST_F(VarDataTests, TypeDecay_ConstIsStripped)
{
    const int x = 42;
    var.set(x);
    EXPECT_TRUE(var.has_type<int>());  // Not const int
}

TEST_F(VarDataTests, TypeDecay_StringLiteralBecomesString)
{
    var.set(std::string("hello"));
    EXPECT_TRUE(var.has_type<std::string>());
}

// =============================================================================
// VarData Edge Cases
// =============================================================================

TEST_F(VarDataTests, OverwriteValue_ChangesType)
{
    var.set(42);
    var.set(std::string("hello"));
    EXPECT_FALSE(var.has_type<int>());
    EXPECT_TRUE(var.has_type<std::string>());
}

TEST_F(VarDataTests, OverwriteValue_UpdatesValue)
{
    var.set(42);
    var.set(100);
    EXPECT_EQ(var.as<int>(), 100);
}

TEST_F(VarDataTests, ResetThenSet_Works)
{
    var.set(42);
    var.reset();
    var.set(std::string("hello"));
    EXPECT_TRUE(var.has_type<std::string>());
    EXPECT_EQ(var.as<std::string>(), "hello");
}
