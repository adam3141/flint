#include <cstdint>

#include <gtest/gtest.h>

#include "flint/meta/enum.hpp"

namespace {

enum class Mineral : std::uint8_t {
  Topaz,
  Ruby,
  Malachite,
};

TEST(MetaEnumTest, NamesEnumerators) {
  EXPECT_EQ(flint::meta::EnumName(Mineral::Ruby), "Ruby");
  EXPECT_EQ(flint::meta::EnumName(static_cast<Mineral>(42)), "");
}

TEST(MetaEnumTest, FindsEnumeratorsByName) {
  EXPECT_EQ(flint::meta::EnumFromName<Mineral>("Malachite"), Mineral::Malachite);
  EXPECT_FALSE(flint::meta::EnumFromName<Mineral>("malachite").has_value());
  EXPECT_EQ(flint::meta::EnumFromNameIgnoreCase<Mineral>("malachite"), Mineral::Malachite);
}

TEST(MetaEnumTest, ListsEnumeratorsInDeclarationOrder) {
  EXPECT_EQ(flint::meta::EnumCount<Mineral>(), 3U);
  const auto values = flint::meta::EnumValues<Mineral>();
  ASSERT_EQ(values.size(), 3U);
  EXPECT_EQ(values[0], Mineral::Topaz);
  EXPECT_EQ(values[2], Mineral::Malachite);
}

}  // namespace
