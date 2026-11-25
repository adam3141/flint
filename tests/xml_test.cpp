#include <string>

#include <gtest/gtest.h>

#include "flint/xml/element.hpp"
#include "flint/xml/parser.hpp"

namespace flint::xml {
namespace {

TEST(XmlTest, ParsesElementsAttributesAndText) {
  const auto document = Parse(R"(<?xml version="1.0"?>
<!-- a comment -->
<Grid Columns="Auto,*" x:Name='Root'>
  <TextBlock Text="Hello" Grid.Row="1"/>
  <Button>Save &amp; close</Button>
</Grid>
)");
  ASSERT_TRUE(document.has_value()) << document.error().Describe();
  EXPECT_EQ(document->name, "Grid");
  EXPECT_EQ(document->GetAttribute("Columns"), "Auto,*");
  EXPECT_EQ(document->GetAttribute("x:Name"), "Root");
  EXPECT_EQ(document->GetAttribute("Missing", "fallback"), "fallback");
  ASSERT_EQ(document->children.size(), 2U);
  EXPECT_EQ(document->children[0].GetAttribute("Grid.Row"), "1");
  EXPECT_EQ(document->children[1].text, "Save & close");
  EXPECT_TRUE(document->text.empty());
}

TEST(XmlTest, RecordsLocations) {
  const auto document = Parse("<Root>\n  <Child  Name=\"a\"/>\n</Root>");
  ASSERT_TRUE(document.has_value());
  const Element& child = document->children.front();
  EXPECT_EQ(child.location, (Location{.line = 2, .column = 3}));
  EXPECT_EQ(child.attributes.front().location, (Location{.line = 2, .column = 11}));
  EXPECT_EQ(child.attributes.front().valueLocation, (Location{.line = 2, .column = 17}));
}

TEST(XmlTest, SplitsPrefixes) {
  const auto document = Parse(R"(<x:String x:Key="Title">agate</x:String>)");
  ASSERT_TRUE(document.has_value());
  EXPECT_EQ(document->GetPrefix(), "x");
  EXPECT_EQ(document->GetLocalName(), "String");
  EXPECT_EQ(document->text, "agate");
}

TEST(XmlTest, DecodesCharacterReferencesAndCdata) {
  const auto document = Parse(R"(<T a="&#65;&#x42;&lt;&gt;&quot;&apos;">&#x2192;<![CDATA[<raw & text>]]></T>)");
  ASSERT_TRUE(document.has_value()) << document.error().Describe();
  EXPECT_EQ(document->GetAttribute("a"), "AB<>\"'");
  EXPECT_EQ(document->text, "\xE2\x86\x92<raw & text>");
}

TEST(XmlTest, ReportsMismatchedTagsWithLocation) {
  const auto document = Parse("<Grid>\n  <Border>\n  </Grid>\n</Border>");
  ASSERT_FALSE(document.has_value());
  EXPECT_EQ(document.error().code, ParseErrorCode::MismatchedEndTag);
  EXPECT_EQ(document.error().location, (Location{.line = 3, .column = 3}));
  EXPECT_EQ(document.error().Describe(), "line 3, column 3: mismatched end tag: expected </Border>, found </Grid>");
}

TEST(XmlTest, RejectsMalformedDocuments) {
  EXPECT_EQ(Parse("").error().code, ParseErrorCode::NoRootElement);
  EXPECT_EQ(Parse("<a>").error().code, ParseErrorCode::UnexpectedEnd);
  EXPECT_EQ(Parse("<a b=\"1\" b=\"2\"/>").error().code, ParseErrorCode::DuplicateAttribute);
  EXPECT_EQ(Parse("<a b=1/>").error().code, ParseErrorCode::UnexpectedCharacter);
  EXPECT_EQ(Parse("<a>&nope;</a>").error().code, ParseErrorCode::InvalidEntity);
  EXPECT_EQ(Parse("<a>&#0;</a>").error().code, ParseErrorCode::InvalidEntity);
  EXPECT_EQ(Parse("<a/><b/>").error().code, ParseErrorCode::TrailingContent);
  EXPECT_EQ(Parse("<!DOCTYPE a><a/>").error().code, ParseErrorCode::DocumentTypeNotAllowed);
  EXPECT_EQ(Parse("<a/>text").error().code, ParseErrorCode::TrailingContent);
  EXPECT_EQ(Parse("text<a/>").error().code, ParseErrorCode::UnexpectedCharacter);
  EXPECT_EQ(Parse("<a><b></a>").error().code, ParseErrorCode::MismatchedEndTag);
  EXPECT_EQ(Parse("<a><b/>").error().code, ParseErrorCode::UnexpectedEnd);
  EXPECT_EQ(Parse("<a b=\"<\"/>").error().code, ParseErrorCode::UnexpectedCharacter);
}

TEST(XmlTest, ReportsTheElementLeftOpen) {
  const auto document = Parse("<a>\n  <b x=\">\"/>\n  <c><!-- </c> --></c>\n");
  ASSERT_FALSE(document.has_value());
  EXPECT_EQ(document.error().code, ParseErrorCode::UnexpectedEnd);
  EXPECT_EQ(document.error().detail, "expected </a>");
}

TEST(XmlTest, LocatesEntityErrorsAndEmptyValues) {
  const auto bad = Parse("<a>\n  <b t=\"x &nope; y\"/>\n</a>");
  ASSERT_FALSE(bad.has_value());
  EXPECT_EQ(bad.error().code, ParseErrorCode::InvalidEntity);
  EXPECT_EQ(bad.error().location, (Location{.line = 2, .column = 11}));

  const auto empty = Parse("<a b=''  c=\"&#10;\"/>");
  ASSERT_TRUE(empty.has_value()) << empty.error().Describe();
  EXPECT_EQ(empty->attributes[0].valueLocation, (Location{.line = 1, .column = 7}));
  EXPECT_EQ(empty->GetAttribute("c"), "\n");
}

TEST(XmlTest, SkipsTheByteOrderMark) {
  const auto document = Parse("\xEF\xBB\xBF<a/>");
  ASSERT_TRUE(document.has_value());
  EXPECT_EQ(document->location, (Location{.line = 1, .column = 1}));
}

TEST(XmlTest, LimitsNestingDepth) {
  std::string deep;
  constexpr int kDepth = 300;
  for (int i = 0; i < kDepth; ++i) {
    deep += "<a>";
  }
  for (int i = 0; i < kDepth; ++i) {
    deep += "</a>";
  }
  EXPECT_EQ(Parse(deep).error().code, ParseErrorCode::NestingTooDeep);
}

}  // namespace
}  // namespace flint::xml
