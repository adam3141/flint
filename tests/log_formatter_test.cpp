#include <chrono>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "flint/log/level.hpp"
#include "flint/log/pattern_formatter.hpp"
#include "flint/log/record.hpp"

namespace {

using namespace flint::log;  // NOLINT(google-build-using-namespace): test brevity

auto SampleRecord() -> Record {
  Record record;
  record.level = kWarning;
  record.message = "disk nearly full";
  record.source =
      SourcePosition{.file = "/src/agate/panel.cpp", .function = "void agate::Panel::Measure(agate::Size)", .line = 42, .column = 3};
  // 2026-11-24T10:15:03.123456Z
  record.timestamp = std::chrono::sys_days{std::chrono::year{2026} / 11 / 24} + std::chrono::hours{10} + std::chrono::minutes{15} +
                     std::chrono::seconds{3} + std::chrono::microseconds{123456};
  record.applicationUptime = std::chrono::milliseconds{1500};
  record.threadId = 77;
  record.threadName = "main";
  record.sequence = 9;
  return record;
}

auto FormatWith(std::string_view pattern) -> std::string {
  auto formatter = PatternFormatter::Create(pattern, TimeZone::Utc);
  EXPECT_TRUE(formatter.has_value());
  std::string out;
  formatter->Format(SampleRecord(), out);
  return out;
}

TEST(PatternFormatterTest, FormatsEveryField) {
  EXPECT_EQ(FormatWith("{level} {severity} {category} {message}"), "Warning 400 app disk nearly full");
  EXPECT_EQ(FormatWith("{file}|{filename}|{line}"), "/src/agate/panel.cpp|panel.cpp|42");
  EXPECT_EQ(FormatWith("{function}|{func}"), "void agate::Panel::Measure(agate::Size)|agate::Panel::Measure");
  EXPECT_EQ(FormatWith("{threadname}:{thread} #{sequence} +{uptime}"), "main:77 #9 +1.500000");
  EXPECT_EQ(FormatWith("{timestamp}"), "2026-11-24 10:15:03.123456");
  EXPECT_EQ(FormatWith("{iso8601}"), "2026-11-24T10:15:03.123456Z");
  EXPECT_EQ(FormatWith("{date} {time}"), "2026-11-24 10:15:03.123");
}

TEST(PatternFormatterTest, AppliesFormatSpecs) {
  EXPECT_EQ(FormatWith("[{level:<9}]"), "[Warning  ]");
  EXPECT_EQ(FormatWith("[{line:>5}]"), "[   42]");
}

TEST(PatternFormatterTest, WritesLiteralBraces) {
  EXPECT_EQ(FormatWith("{{{level}}}"), "{Warning}");
}

TEST(PatternFormatterTest, RejectsUnknownFieldsAndBadSpecs) {
  EXPECT_EQ(PatternFormatter::Create("{nope}").error().code, PatternError::Code::UnknownField);
  EXPECT_EQ(PatternFormatter::Create("{level").error().code, PatternError::Code::UnterminatedField);
  EXPECT_EQ(PatternFormatter::Create("{level:%%}").error().code, PatternError::Code::InvalidSpec);
}

TEST(PatternFormatterTest, ColoursLevelAndDimsSourceWithAnsi) {
  auto formatter = PatternFormatter::Create("{level} {filename}", TimeZone::Utc, Colouring::Ansi);
  ASSERT_TRUE(formatter.has_value());
  std::string out;
  formatter->Format(SampleRecord(), out);
  EXPECT_EQ(out,
            std::string{AnsiSequence(Colour::BrightYellow)} + "Warning" + std::string{kAnsiReset} + " " + std::string{kAnsiDim} +
                "panel.cpp" + std::string{kAnsiReset});
}

TEST(ShortFunctionNameTest, StripsReturnTypeAndParameters) {
  EXPECT_EQ(ShortFunctionName("int main()"), "main");
  EXPECT_EQ(ShortFunctionName("void ns::Type::Method(int) const"), "ns::Type::Method");
  EXPECT_EQ(ShortFunctionName("ns::Type::Type(std::string)"), "ns::Type::Type");
  EXPECT_EQ(ShortFunctionName("std::vector<int> ns::Make<std::pair<int, int> >(std::size_t)"), "ns::Make<std::pair<int, int> >");
  EXPECT_EQ(ShortFunctionName("bool ns::Type::operator()(int)"), "ns::Type::operator()");
  EXPECT_EQ(ShortFunctionName("const char* ns::Name()"), "ns::Name");
}

}  // namespace
