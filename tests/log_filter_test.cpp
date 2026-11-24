#include <gtest/gtest.h>

#include "flint/log/category.hpp"
#include "flint/log/filter.hpp"
#include "flint/log/level.hpp"

namespace {

using namespace flint::log;  // NOLINT(google-build-using-namespace): test brevity

TEST(LogLevelTest, StandardLevelsHaveNamesAndOrder) {
  EXPECT_LT(kTrace, kDebug);
  EXPECT_LT(kError, kFatal);
  EXPECT_EQ(LevelName(kWarning), "Warning");
  EXPECT_EQ(FindLevel("warning"), kWarning);
  EXPECT_EQ(FindLevel("OFF"), kOff);
}

TEST(LogLevelTest, CustomLevelsSitBetweenStandardLevels) {
  const auto verbose = RegisterLevel("FilterTestVerbose", 150, Colour::Magenta);
  ASSERT_TRUE(verbose.has_value());
  EXPECT_GT(*verbose, kTrace);
  EXPECT_LT(*verbose, kDebug);
  EXPECT_EQ(LevelName(*verbose), "FilterTestVerbose");
  EXPECT_EQ(LevelColour(*verbose), Colour::Magenta);
  EXPECT_EQ(FindLevel("filtertestverbose"), *verbose);
}

TEST(LogLevelTest, RegisteringTheSameLevelTwiceIsIdempotent) {
  ASSERT_TRUE(RegisterLevel("FilterTestNotice", 350, Colour::Blue).has_value());
  EXPECT_TRUE(RegisterLevel("FilterTestNotice", 350, Colour::Blue).has_value());
}

TEST(LogLevelTest, RejectsConflictingLevels) {
  EXPECT_EQ(RegisterLevel("Info", 301).error(), LevelError::NameInUse);
  EXPECT_EQ(RegisterLevel("Louder", kError.severity).error(), LevelError::SeverityInUse);
  EXPECT_EQ(RegisterLevel("", 123).error(), LevelError::EmptyName);
  EXPECT_EQ(RegisterLevel("Zero", 0).error(), LevelError::ReservedSeverity);
}

TEST(LogCategoryTest, RegistersAndFindsCategoriesIgnoringCase) {
  const auto layout = RegisterCategory("FilterTestLayout");
  ASSERT_TRUE(layout.has_value());
  EXPECT_EQ(RegisterCategory("filtertestlayout"), layout);
  EXPECT_EQ(FindCategory("FILTERTESTLAYOUT"), layout);
  EXPECT_EQ(CategoryName(*layout), "FilterTestLayout");
  EXPECT_EQ(CategoryName(kDefaultCategory), "app");
}

TEST(LogFilterTest, AppliesDefaultMinimum) {
  const Filter filter{kInfo};
  EXPECT_FALSE(filter.Accepts(kDebug, kDefaultCategory));
  EXPECT_TRUE(filter.Accepts(kInfo, kDefaultCategory));
}

TEST(LogFilterTest, CategoryOverridesWinOverTheDefault) {
  const Category render = *RegisterCategory("FilterTestRender");
  const Category layout = *RegisterCategory("FilterTestLayout2");
  Filter filter{kInfo};
  filter.SetCategoryMinimum(render, kOff);
  filter.SetCategoryMinimum(layout, kTrace);
  EXPECT_FALSE(filter.Accepts(kFatal, render));
  EXPECT_TRUE(filter.Accepts(kTrace, layout));
  EXPECT_EQ(filter.LowestThreshold(), kTrace);
  filter.ClearCategoryMinimum(render);
  EXPECT_TRUE(filter.Accepts(kInfo, render));
}

}  // namespace
