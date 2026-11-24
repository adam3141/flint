#include <filesystem>
#include <random>
#include <string>

#include <gtest/gtest.h>

#include "flint/log/category.hpp"
#include "flint/log/config.hpp"
#include "flint/log/level.hpp"
#include "flint/log/logger.hpp"

namespace {

using namespace flint::log;  // NOLINT(google-build-using-namespace): test brevity

TEST(LogConfigTest, RegistersLevelsAndCategoriesAndAppliesFilters) {
  const auto directory = std::filesystem::temp_directory_path() / ("flint-config-" + std::to_string(std::random_device{}()));
  const auto jsonPath = (directory / "app.jsonl").generic_string();
  const std::string config = R"({
    // Custom severity between Trace and Debug.
    "levels": [ { "name": "ConfigTestVerbose", "severity": 150, "colour": "brightMagenta" } ],
    "categories": [ "ConfigTestLayout" ],
    "sinks": [
      { "type": "json", "path": ")" +
                             jsonPath + R"(", "minLevel": "ConfigTestVerbose",
        "categories": { "ConfigTestRender": "Off", "ConfigTestLayout": "Trace" } },
    ],
  })";

  Logger logger{{.mode = Mode::Synchronous}};
  const auto result = ConfigureFromText(logger, config);
  ASSERT_TRUE(result.has_value()) << result.error().Describe();

  const auto verbose = FindLevel("ConfigTestVerbose");
  ASSERT_TRUE(verbose.has_value());
  EXPECT_EQ(LevelColour(*verbose), Colour::BrightMagenta);
  EXPECT_TRUE(FindCategory("ConfigTestLayout").has_value());

  EXPECT_TRUE(logger.IsEnabled(*verbose, kDefaultCategory));
  EXPECT_FALSE(logger.IsEnabled(kTrace, kDefaultCategory));
  EXPECT_TRUE(logger.IsEnabled(kTrace, *FindCategory("ConfigTestLayout")));
  EXPECT_FALSE(logger.IsEnabled(kFatal, *FindCategory("ConfigTestRender")));
  logger.RemoveAllSinks();
  std::filesystem::remove_all(directory);
}

TEST(LogConfigTest, ReportsWhereTheConfigurationIsWrong) {
  Logger logger{{.mode = Mode::Synchronous}};
  const auto unknownType = ConfigureFromText(logger, R"({"sinks": [ {"type": "console"}, {"type": "carrier-pigeon"} ]})");
  ASSERT_FALSE(unknownType.has_value());
  EXPECT_EQ(unknownType.error().code, ConfigError::Code::UnknownSinkType);
  EXPECT_EQ(unknownType.error().where, "/sinks/1/type");

  const auto badLevel = ConfigureFromText(logger, R"({"sinks": [ {"type": "console", "minLevel": "Loud"} ]})");
  ASSERT_FALSE(badLevel.has_value());
  EXPECT_EQ(badLevel.error().code, ConfigError::Code::UnknownLevel);
  EXPECT_EQ(badLevel.error().where, "/sinks/0/minLevel");

  const auto badColour = ConfigureFromText(logger, R"({"sinks": [ {"type": "console", "colour": "sometimes"} ]})");
  ASSERT_FALSE(badColour.has_value());
  EXPECT_EQ(badColour.error().where, "/sinks/0/colour");

  const auto malformed = ConfigureFromText(logger, R"({"sinks": [)");
  ASSERT_FALSE(malformed.has_value());
  EXPECT_EQ(malformed.error().code, ConfigError::Code::MalformedJson);
}

TEST(LogConfigTest, LeavesTheLoggerUnchangedOnFailure) {
  Logger logger{{.mode = Mode::Synchronous}};
  ASSERT_TRUE(ConfigureFromText(logger, R"({"sinks": [ {"type": "console", "minLevel": "Error"} ]})").has_value());
  ASSERT_FALSE(ConfigureFromText(logger, R"({"sinks": [ {"type": "console", "minLevel": "Trace"}, {"type": "?"} ]})").has_value());
  EXPECT_FALSE(logger.IsEnabled(kWarning, kDefaultCategory));
  EXPECT_TRUE(logger.IsEnabled(kError, kDefaultCategory));
}

TEST(LogConfigTest, ReportsMissingFiles) {
  Logger logger{{.mode = Mode::Synchronous}};
  const auto result = ConfigureFromFile(logger, "/definitely/not/here.json");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code, ConfigError::Code::CannotReadFile);
}

}  // namespace
