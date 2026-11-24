#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>

#include "flint/meta/reflection.hpp"

namespace flint::meta {

/// An enumeration type.
template <typename Type>
concept Enumeration = std::is_enum_v<Type>;

/// Returns the declared name of an enumerator, for example "Warning" for `Level::Warning`. Values that do not match an
/// enumerator return an empty view. The view refers to static storage.
/// @param value The enumerator.
template <Enumeration Enum>
[[nodiscard]] constexpr auto EnumName(Enum value) noexcept -> std::string_view;

/// Finds the enumerator with a given declared name. Matching is case-sensitive.
/// @param name The enumerator name.
/// @return The enumerator, or nothing if no enumerator has that name.
template <Enumeration Enum>
[[nodiscard]] constexpr auto EnumFromName(std::string_view name) noexcept -> std::optional<Enum>;

/// Finds the enumerator with a given declared name, ignoring ASCII case. Used when reading configuration files.
/// @param name The enumerator name.
/// @return The enumerator, or nothing if no enumerator has that name.
template <Enumeration Enum>
[[nodiscard]] constexpr auto EnumFromNameIgnoreCase(std::string_view name) noexcept -> std::optional<Enum>;

/// Returns every enumerator of an enumeration, in declaration order.
template <Enumeration Enum>
[[nodiscard]] constexpr auto EnumValues() noexcept -> std::span<const Enum>;

/// Returns the number of enumerators in an enumeration.
template <Enumeration Enum>
[[nodiscard]] constexpr auto EnumCount() noexcept -> std::size_t;

}  // namespace flint::meta

#if FLINT_HAS_REFLECTION
#include "flint/meta/detail/enum_impl.hpp"
#else
// Tooling-only stub bodies for clangd and clang-tidy, which cannot parse reflection. Never compiled into a binary: see
// flint/meta/reflection.hpp.
namespace flint::meta {

template <Enumeration Enum>
constexpr auto EnumName(Enum /*value*/) noexcept -> std::string_view {
  return {};
}

template <Enumeration Enum>
constexpr auto EnumFromName(std::string_view /*name*/) noexcept -> std::optional<Enum> {
  return std::nullopt;
}

template <Enumeration Enum>
constexpr auto EnumFromNameIgnoreCase(std::string_view /*name*/) noexcept -> std::optional<Enum> {
  return std::nullopt;
}

template <Enumeration Enum>
constexpr auto EnumValues() noexcept -> std::span<const Enum> {
  return {};
}

template <Enumeration Enum>
constexpr auto EnumCount() noexcept -> std::size_t {
  return 0;
}

}  // namespace flint::meta
#endif
