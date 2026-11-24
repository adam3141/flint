#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

#include "flint/json/value.hpp"
#include "flint/log/logger.hpp"

namespace flint::log {

/// Why a logging configuration could not be applied.
struct ConfigError {
  enum class Code : std::uint8_t {
    CannotReadFile,
    MalformedJson,
    WrongType,
    MissingField,
    UnknownSinkType,
    UnknownLevel,
    UnknownColour,
    InvalidValue,
    InvalidLevel,
    InvalidCategory,
    InvalidPattern,
    CannotOpenSink,
  };

  Code code{Code::InvalidValue};
  /// JSON Pointer to the offending value, for example "/sinks/1/minLevel".
  std::string where;
  /// Extra detail, such as the parse error position or the OS error.
  std::string detail;

  [[nodiscard]] auto Describe() const -> std::string;
};

/// Replaces a logger's sinks with those described by a configuration document, and registers any custom levels and
/// categories it declares. On failure the logger is left unchanged, although levels and categories registered before
/// the failure remain registered. See docs/api/logging.md for the schema.
///
/// @code{.json}
/// {
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
/// @param logger The logger to configure.
/// @param config The parsed configuration document.
[[nodiscard]] auto Configure(Logger& logger, const json::Value& config) -> std::expected<void, ConfigError>;

/// Parses a configuration document (comments and trailing commas allowed) and applies it with Configure.
/// @param logger The logger to configure.
/// @param text The JSON text.
[[nodiscard]] auto ConfigureFromText(Logger& logger, std::string_view text) -> std::expected<void, ConfigError>;

/// Reads a configuration file and applies it with Configure.
/// @param logger The logger to configure.
/// @param path The configuration file.
[[nodiscard]] auto ConfigureFromFile(Logger& logger, const std::filesystem::path& path) -> std::expected<void, ConfigError>;

}  // namespace flint::log
