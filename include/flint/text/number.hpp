#pragma once

#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>

namespace flint::text {

/// Why text could not be read as a number.
enum class NumberError : std::uint8_t {
  NotANumber,
  OutOfRange,
};

/// Base 10, the default for ParseInteger.
inline constexpr int kDecimal = 10;

namespace detail {

// One place for the pointer range that <charconv> requires.
[[nodiscard]] inline auto EndOf(std::string_view text) noexcept -> const char* {
  return text.data() + text.size();  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): <charconv> takes [first, last)
}

template <std::size_t Size>
[[nodiscard]] auto EndOf(std::array<char, Size>& buffer) noexcept -> char* {
  return buffer.data() + buffer.size();  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic): <charconv> takes [first, last)
}

}  // namespace detail

/// Parses the whole of `text` as an integer. Leading '+', whitespace and trailing characters are rejected.
/// @param text The digits, with an optional leading '-' for signed types.
/// @param base The number base, 2 to 36.
template <std::integral Integer>
[[nodiscard]] auto ParseInteger(std::string_view text, int base = kDecimal) -> std::expected<Integer, NumberError> {
  Integer value{};
  const char* last = detail::EndOf(text);
  // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage): `last` bounds the range
  const auto [end, error] = std::from_chars(text.data(), last, value, base);
  if (error == std::errc::result_out_of_range) {
    return std::unexpected(NumberError::OutOfRange);
  }
  if (error != std::errc{} || end != last) {
    return std::unexpected(NumberError::NotANumber);
  }
  return value;
}

/// Parses the whole of `text` as a floating-point number in general (fixed or scientific) notation.
/// "inf" and "nan" are accepted, as std::from_chars accepts them.
/// @param text The number.
template <std::floating_point Floating>
[[nodiscard]] auto ParseFloating(std::string_view text) -> std::expected<Floating, NumberError> {
  Floating value{};
  const char* last = detail::EndOf(text);
  // NOLINTNEXTLINE(bugprone-suspicious-stringview-data-usage): `last` bounds the range
  const auto [end, error] = std::from_chars(text.data(), last, value);
  if (error == std::errc::result_out_of_range) {
    return std::unexpected(NumberError::OutOfRange);
  }
  if (error != std::errc{} || end != last) {
    return std::unexpected(NumberError::NotANumber);
  }
  return value;
}

/// Appends the shortest text that round-trips `value` (std::to_chars without a format) to `out`.
/// @param out The buffer to append to.
/// @param value An integer or floating-point number.
template <typename Number>
  requires std::integral<Number> || std::floating_point<Number>
auto AppendNumber(std::string& out, Number value) -> void {
  // Enough for any 64-bit integer or the shortest round-trip form of a double.
  constexpr std::size_t kBufferSize = 32;
  std::array<char, kBufferSize> buffer{};
  const auto result = std::to_chars(buffer.data(), detail::EndOf(buffer), value);
  out.append(buffer.data(), result.ptr);
}

}  // namespace flint::text
