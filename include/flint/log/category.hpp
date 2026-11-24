#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace flint::log {

/// A log category (channel), such as "layout", "render" or "binding". Categories are independent of severity: each sink
/// can enable, disable or set a separate minimum level for each category.
struct Category {
  std::uint16_t id{0};

  friend constexpr auto operator<=>(const Category& left, const Category& right) = default;
};

/// The category used when none is given. Its name is "app".
inline constexpr Category kDefaultCategory{.id = 0};

/// The most categories a process can register, including the default category.
inline constexpr std::size_t kMaxCategories = 1024;

/// Why a category could not be registered.
enum class CategoryError : std::uint8_t {
  EmptyName,
  TooManyCategories,
};

/// Registers a category, or returns the existing one with the same name. Names are compared without regard to ASCII
/// case.
/// @param name The category name.
/// @return The category, or why it could not be registered.
[[nodiscard]] auto RegisterCategory(std::string_view name) -> std::expected<Category, CategoryError>;

/// Finds a registered category by name, ignoring ASCII case.
/// @param name The category name.
[[nodiscard]] auto FindCategory(std::string_view name) -> std::optional<Category>;

/// Returns the name of a registered category. Unregistered identifiers return "?". The view is valid for the life of the
/// process.
/// @param category The category.
[[nodiscard]] auto CategoryName(Category category) -> std::string_view;

/// Returns the names of every registered category, in registration order.
[[nodiscard]] auto RegisteredCategories() -> std::vector<std::string>;

/// Returns a human-readable description of a category error.
[[nodiscard]] auto Describe(CategoryError error) -> std::string_view;

}  // namespace flint::log
