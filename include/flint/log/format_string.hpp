#pragma once

#include <concepts>
#include <format>
#include <source_location>
#include <string_view>
#include <type_traits>

namespace flint::log {

/// A compile-time checked format string that also captures where it was written.
///
/// Logging functions take this as their first parameter, so the caller's source location is recorded without a
/// macro: `flint::log::Info("Loaded {} files", count)` records the file, line and function of that call.
template <typename... Args>
struct BasicFormatString {
  /// Captures the format string and the location of the call that wrote it.
  /// Implicit, so a string literal converts at the call site; consteval, so the format is checked at compile time.
  template <typename Text>
    requires std::convertible_to<const Text&, std::string_view>
  // NOLINTNEXTLINE(google-explicit-constructor): implicit conversion from a literal is the point of this type
  consteval BasicFormatString(const Text& text, std::source_location where = std::source_location::current())
      : format{text}, location{where} {}

  std::format_string<Args...> format;
  std::source_location location;
};

/// The format-string parameter type for logging functions. std::type_identity_t stops the format string from taking
/// part in template argument deduction, so Args are deduced from the arguments alone.
template <typename... Args>
using FormatString = BasicFormatString<std::type_identity_t<Args>...>;

}  // namespace flint::log
