#pragma once

// Reflection implementation of flint/meta/enum.hpp, which includes this file (only) when FLINT_HAS_REFLECTION is 1.
// clangd cannot parse this file until LLVM implements P2996; see flint ADR-0003.

#include <array>
#include <cstddef>
#include <meta>
#include <optional>
#include <span>
#include <string_view>

#include "flint/text/ascii.hpp"

namespace flint::meta {
namespace detail {

template <Enumeration Enum>
inline constexpr auto kEnumerators = std::define_static_array(std::meta::enumerators_of(^^Enum));

template <Enumeration Enum>
inline constexpr auto kEnumValues = [] {
  std::array<Enum, kEnumerators<Enum>.size()> values{};
  std::size_t index = 0;
  template for (constexpr auto enumerator : kEnumerators<Enum>) {
    values[index++] = [:enumerator:];
  }
  return values;
}();

}  // namespace detail

template <Enumeration Enum>
constexpr auto EnumName(Enum value) noexcept -> std::string_view {
  template for (constexpr auto enumerator : detail::kEnumerators<Enum>) {
    if (value == [:enumerator:]) {
      return std::define_static_string(std::meta::identifier_of(enumerator));
    }
  }
  return {};
}

template <Enumeration Enum>
constexpr auto EnumFromName(std::string_view name) noexcept -> std::optional<Enum> {
  template for (constexpr auto enumerator : detail::kEnumerators<Enum>) {
    if (name == std::meta::identifier_of(enumerator)) {
      return [:enumerator:];
    }
  }
  return std::nullopt;
}

template <Enumeration Enum>
constexpr auto EnumFromNameIgnoreCase(std::string_view name) noexcept -> std::optional<Enum> {
  template for (constexpr auto enumerator : detail::kEnumerators<Enum>) {
    if (text::EqualsIgnoreCase(name, std::meta::identifier_of(enumerator))) {
      return [:enumerator:];
    }
  }
  return std::nullopt;
}

template <Enumeration Enum>
constexpr auto EnumValues() noexcept -> std::span<const Enum> {
  return detail::kEnumValues<Enum>;
}

template <Enumeration Enum>
constexpr auto EnumCount() noexcept -> std::size_t {
  return detail::kEnumerators<Enum>.size();
}

}  // namespace flint::meta
