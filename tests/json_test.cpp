#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "flint/json/parser.hpp"
#include "flint/json/value.hpp"
#include "flint/json/writer.hpp"

namespace {

using flint::json::Array;
using flint::json::Object;
using flint::json::Parse;
using flint::json::ParseErrorCode;
using flint::json::Serialise;
using flint::json::Value;

TEST(JsonParseTest, ParsesScalars) {
  EXPECT_TRUE(Parse("null")->IsNull());
  EXPECT_EQ(Parse("true")->AsBool(), true);
  EXPECT_EQ(Parse("false")->AsBool(), false);
  EXPECT_EQ(Parse("-42")->AsInteger(), -42);
  EXPECT_DOUBLE_EQ(*Parse("2.5e3")->AsDouble(), 2500.0);
  EXPECT_EQ(Parse(R"("hi")")->AsString(), "hi");
}

TEST(JsonParseTest, KeepsLargeIntegersExactAndFallsBackToDoubleBeyondInt64) {
  EXPECT_EQ(Parse("9223372036854775807")->AsInteger(), INT64_MAX);
  const auto huge = Parse("18446744073709551616");
  ASSERT_TRUE(huge.has_value());
  EXPECT_EQ(huge->GetKind(), flint::json::Kind::Double);
}

TEST(JsonParseTest, DecodesEscapesAndSurrogatePairs) {
  const auto value = Parse(R"("a\"b\\c\né😀")");
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(*value->AsString(), "a\"b\\c\n\xC3\xA9\xF0\x9F\x98\x80");
}

TEST(JsonParseTest, PreservesObjectMemberOrder) {
  const auto value = Parse(R"({"z":1,"a":2,"m":3})");
  ASSERT_TRUE(value.has_value());
  const auto members = value->AsObject()->Members();
  ASSERT_EQ(members.size(), 3U);
  EXPECT_EQ(members[0].key, "z");
  EXPECT_EQ(members[1].key, "a");
  EXPECT_EQ(members[2].key, "m");
}

TEST(JsonParseTest, ReportsLineAndColumnOfErrors) {
  const auto value = Parse("{\n  \"a\": 1,\n  \"b\": ?\n}");
  ASSERT_FALSE(value.has_value());
  EXPECT_EQ(value.error().code, ParseErrorCode::UnexpectedCharacter);
  EXPECT_EQ(value.error().line, 3U);
  EXPECT_EQ(value.error().column, 8U);
}

TEST(JsonParseTest, RejectsCommentsAndTrailingCommasByDefault) {
  EXPECT_FALSE(Parse("[1, 2,]").has_value());
  EXPECT_FALSE(Parse("// c\n1").has_value());
}

TEST(JsonParseTest, AcceptsCommentsAndTrailingCommasInConfigMode) {
  const auto value = Parse("{ // comment\n \"a\": [1, 2,], /* block */ }", flint::json::kConfigParseOptions);
  ASSERT_TRUE(value.has_value()) << value.error().Describe();
  EXPECT_EQ(value->Find("a")->AsArray()->size(), 2U);
}

TEST(JsonParseTest, RejectsInvalidNumbers) {
  EXPECT_FALSE(Parse("01").has_value());
  EXPECT_FALSE(Parse("1.").has_value());
  EXPECT_FALSE(Parse("+1").has_value());
  EXPECT_FALSE(Parse("NaN").has_value());
}

TEST(JsonParseTest, RejectsDeepNesting) {
  const std::string deep(1000, '[');
  const auto value = Parse(deep);
  ASSERT_FALSE(value.has_value());
  EXPECT_EQ(value.error().code, ParseErrorCode::NestingTooDeep);
}

TEST(JsonWriteTest, RoundTripsThroughSerialise) {
  const Value original = Object{{"name", "Topaz"}, {"dark", true}, {"scale", 1.5}, {"list", Array{1, "two", nullptr}}};
  const auto text = Serialise(original);
  EXPECT_EQ(text, R"({"name":"Topaz","dark":true,"scale":1.5,"list":[1,"two",null]})");
  EXPECT_EQ(*Parse(text), original);
}

TEST(JsonWriteTest, EscapesControlCharacters) {
  EXPECT_EQ(Serialise(Value{"tab\there\x01"}), R"("tab\there\u0001")");
}

TEST(JsonWriteTest, IndentsWhenAsked) {
  const Value value = Object{{"a", Array{1, 2}}};
  EXPECT_EQ(Serialise(value, {.indent = 2}), "{\n  \"a\": [\n    1,\n    2\n  ]\n}");
}

}  // namespace
