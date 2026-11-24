#pragma once

#include <algorithm>
#include <string>
#include <string_view>

namespace flint::text {

/// Lower-cases an ASCII letter; other characters are returned unchanged.
[[nodiscard]] constexpr auto ToLowerAscii(char character) noexcept -> char {
  return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
}

/// Compares two strings, treating ASCII letters case-insensitively. Non-ASCII bytes must match exactly.
[[nodiscard]] constexpr auto EqualsIgnoreCase(std::string_view left, std::string_view right) noexcept -> bool {
  return std::ranges::equal(left, right, [](char a, char b) { return ToLowerAscii(a) == ToLowerAscii(b); });
}

/// Returns a copy with ASCII letters lower-cased.
[[nodiscard]] inline auto ToLowerAscii(std::string_view text) -> std::string {
  std::string result{text};
  std::ranges::transform(result, result.begin(), [](char character) { return ToLowerAscii(character); });
  return result;
}

}  // namespace flint::text
