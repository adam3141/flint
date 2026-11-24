#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

#include "flint/xml/element.hpp"

namespace flint::xml {

/// Why parsing failed.
enum class ParseErrorCode : std::uint8_t {
  UnexpectedEnd,
  UnexpectedCharacter,
  MismatchedEndTag,
  DuplicateAttribute,
  InvalidEntity,
  NoRootElement,
  TrailingContent,
  NestingTooDeep,
};

/// A parse failure and where it happened.
struct ParseError {
  ParseErrorCode code{ParseErrorCode::UnexpectedEnd};
  Location location;
  /// Extra detail, for example the expected end tag.
  std::string detail;

  /// Returns a readable description, such as "line 3, column 7: expected </Grid>, found </Border>".
  [[nodiscard]] auto Describe() const -> std::string;
};

/// Parses an XML document into its root element (flint ADR-0010).
///
/// Supports elements, attributes (single or double quotes), text, CDATA sections, comments, processing instructions,
/// the XML declaration and the predefined and numeric character entities. DTDs and external entities are not
/// supported: a `<!DOCTYPE` is an error, which also rules out entity-expansion attacks.
///
/// @param text The UTF-8 document.
/// @return The root element, or where and why parsing failed.
[[nodiscard]] auto Parse(std::string_view text) -> std::expected<Element, ParseError>;

}  // namespace flint::xml
