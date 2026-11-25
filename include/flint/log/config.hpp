#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "flint/log/logger.hpp"

namespace flint::log {

/// A level in a configuration: a name ("Debug", or a custom level) or a severity (250).
using LevelSetting = std::variant<std::string, std::uint16_t>;

// The empty initialisers let callers use designated initialisers for just the fields they need: GCC's
// -Wmissing-field-initializers warns about omitted members that have no initialiser.
// NOLINTBEGIN(readability-redundant-member-init)

/// A custom level to register: `{ "name": "Verbose", "severity": 150, "colour": "magenta" }`.
struct LevelDefinition {
  std::string name{};
  std::optional<std::uint16_t> severity{};
  /// A Colour name, ignoring case.
  std::optional<std::string> colour{};
};

/// One sink. `type` chooses the sink; each type reads only its own fields.
struct SinkConfig {
  /// "console", "file", "json", "network" or "tracy", ignoring case.
  std::string type{};
  /// The lowest level written. Default: everything.
  std::optional<LevelSetting> minLevel{};
  /// Per-category minimum levels, overriding minLevel: `{ "render": "Off" }`.
  std::map<std::string, LevelSetting> categories{};

  // console
  /// "stdout" (default) or "stderr".
  std::optional<std::string> stream{};
  /// A ColourMode name: "auto" (default), "always" or "never".
  std::optional<std::string> colour{};
  // console and file
  /// A PatternFormatter pattern.
  std::optional<std::string> pattern{};
  /// A TimeZone name: "local" (default) or "utc".
  std::optional<std::string> timeZone{};
  // file and json
  /// Required for file and json sinks.
  std::optional<std::string> path{};
  std::optional<std::uint64_t> maxBytes{};
  std::optional<std::uint16_t> maxFiles{};
  // network
  std::optional<std::string> host{};
  std::optional<std::uint16_t> port{};
  std::optional<std::uint32_t> bufferCapacity{};
  std::optional<std::uint32_t> reconnectDelayMs{};
  std::optional<std::uint32_t> maxReconnectDelayMs{};
  std::optional<std::uint32_t> connectTimeoutMs{};
};

/// A logging configuration (ADR-0006, ADR-0012). It is a plain struct that glaze reads, so an application can embed
/// it in its own configuration and read both in one go.
///
/// @code{.json}
/// {
///   // Comments are allowed.
///   "levels":     [ { "name": "Verbose", "severity": 150, "colour": "magenta" } ],
///   "categories": [ "layout", "render" ],
///   "sinks": [
///     { "type": "console", "minLevel": "Debug", "categories": { "render": "Off" } },
///     { "type": "file",    "path": "logs/app.log", "maxBytes": 10485760, "maxFiles": 5 },
///     { "type": "json",    "path": "logs/app.jsonl" },
///     { "type": "network", "host": "127.0.0.1", "port": 9020 }
///   ]
/// }
/// @endcode
struct LoggingConfig {
  std::vector<LevelDefinition> levels{};
  std::vector<std::string> categories{};
  std::vector<SinkConfig> sinks{};
};

// NOLINTEND(readability-redundant-member-init)

/// Why a logging configuration could not be applied.
struct ConfigError {
  enum class Code : std::uint8_t {
    CannotReadFile,
    MalformedJson,
    MissingField,
    UnknownSinkType,
    UnknownLevel,
    InvalidValue,
    InvalidLevel,
    InvalidCategory,
    InvalidPattern,
    CannotOpenSink,
  };

  Code code{Code::InvalidValue};
  /// Where the problem is: "line 3, column 7" when the JSON could not be read, or a JSON Pointer to the offending value
  /// when it was read but does not make sense, for example "/sinks/1/minLevel".
  std::string where;
  /// Extra detail, such as glaze's error and the offending line, or the OS error.
  std::string detail;

  [[nodiscard]] auto Describe() const -> std::string;
};

/// Replaces a logger's sinks with those described by a configuration, and registers any custom levels and categories
/// it declares. On failure the logger is left unchanged, although levels and categories registered before the failure
/// remain registered. See docs/api/logging.md for the schema.
/// @param logger The logger to configure.
/// @param config The configuration.
[[nodiscard]] auto Configure(Logger& logger, const LoggingConfig& config) -> std::expected<void, ConfigError>;

/// Reads a configuration (JSON with comments; unknown keys are errors) and applies it with Configure.
/// @param logger The logger to configure.
/// @param text The JSON text.
[[nodiscard]] auto ConfigureFromText(Logger& logger, std::string_view text) -> std::expected<void, ConfigError>;

/// Reads a configuration file and applies it with Configure.
/// @param logger The logger to configure.
/// @param path The configuration file.
[[nodiscard]] auto ConfigureFromFile(Logger& logger, const std::filesystem::path& path) -> std::expected<void, ConfigError>;

}  // namespace flint::log
