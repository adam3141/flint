#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace flint::xml {

/// A position in an XML document. Line and column are 1-based; column counts bytes.
struct Location {
  std::uint32_t line{1};
  std::uint32_t column{1};

  friend constexpr auto operator==(const Location& left, const Location& right) -> bool = default;
};

/// An attribute: `name="value"`, with entities already decoded.
struct Attribute {
  /// The qualified name as written, including any prefix: "x:Name", "Grid.Row".
  std::string name;
  std::string value;
  /// Where the attribute's name starts.
  Location location;
  /// Where the value starts (after the opening quote).
  Location valueLocation;
};

/// An element with its attributes, child elements and text (flint ADR-0011).
///
/// Text directly inside the element is joined into `text`; text that is only whitespace is dropped. That suits markup
/// (agate, configuration), where mixed content is rare. Comments, processing instructions and the XML declaration are
/// skipped.
struct Element {
  /// The qualified name as written, including any prefix: "Button", "x:String", "Border.Background".
  std::string name;
  std::vector<Attribute> attributes;
  std::vector<Element> children;
  std::string text;
  /// Where the start tag begins (its '<').
  Location location;

  /// Finds an attribute by its qualified name, or nullptr.
  [[nodiscard]] auto FindAttribute(std::string_view attributeName) const noexcept -> const Attribute*;

  /// Returns an attribute's value, or `fallback` if it is absent.
  [[nodiscard]] auto GetAttribute(std::string_view attributeName, std::string_view fallback = {}) const -> std::string_view;

  /// The name without a namespace prefix: "String" for "x:String".
  [[nodiscard]] auto GetLocalName() const noexcept -> std::string_view;

  /// The namespace prefix, or empty: "x" for "x:String".
  [[nodiscard]] auto GetPrefix() const noexcept -> std::string_view;
};

}  // namespace flint::xml
