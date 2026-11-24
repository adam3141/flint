#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "flint/json/value.hpp"

namespace flint::json {

/// Why parsing failed.
enum class ParseErrorCode : std::uint8_t {
  UnexpectedEnd,
  UnexpectedCharacter,
  InvalidNumber,
  InvalidEscape,
  InvalidUnicode,
  ControlCharacterInString,
  TrailingCharacters,
  NestingTooDeep,
};

/// A parse failure and where it happened. Line and column are 1-based; column counts bytes.
struct ParseError {
  ParseErrorCode code{ParseErrorCode::UnexpectedEnd};
  std::size_t offset{0};
  std::size_t line{1};
  std::size_t column{1};

  /// Returns a human-readable description, such as "unexpected character at line 3, column 7".
  [[nodiscard]] auto Describe() const -> std::string;
};

/// Extensions to strict JSON (RFC 8259). Configuration files allow both; data exchanged with other programs allows
/// neither.
struct ParseOptions {
  bool allowComments{false};
  bool allowTrailingCommas{false};
};

/// The relaxed options used for silica configuration files: // and /* */ comments, and trailing commas.
inline constexpr ParseOptions kConfigParseOptions{.allowComments = true, .allowTrailingCommas = true};

/// Parses a JSON document.
/// @param text The UTF-8 document.
/// @param options Which extensions to accept.
/// @return The parsed value, or where and why parsing failed.
[[nodiscard]] auto Parse(std::string_view text, ParseOptions options = {}) -> std::expected<Value, ParseError>;

}  // namespace flint::json
