#pragma once

#include <utility>
#include <vector>

#include "flint/log/category.hpp"
#include "flint/log/level.hpp"

namespace flint::log {

/// Decides which records a sink receives, by severity and by category.
///
/// A record passes when its severity is at least the threshold for its category. The threshold is the category's own
/// minimum if one is set, and the filter's default minimum otherwise. Setting a category's minimum to kOff disables
/// that category.
class Filter {
public:
  /// @param minimum The default minimum level for every category.
  explicit Filter(Level minimum = kTrace) noexcept;

  auto SetMinimum(Level minimum) noexcept -> void;

  [[nodiscard]] auto GetMinimum() const noexcept -> Level;

  /// Overrides the minimum level for one category. Use kOff to disable the category.
  /// @param category The category to override.
  /// @param minimum Its minimum level.
  auto SetCategoryMinimum(Category category, Level minimum) -> void;

  /// Removes a category override, so the category uses the default minimum again.
  /// @param category The category.
  auto ClearCategoryMinimum(Category category) -> void;

  /// Returns the minimum level that applies to a category.
  /// @param category The category.
  [[nodiscard]] auto Threshold(Category category) const noexcept -> Level;

  /// Returns the lowest threshold over every category. Records below it are rejected by this filter whatever their
  /// category.
  [[nodiscard]] auto LowestThreshold() const noexcept -> Level;

  /// Reports whether a record with this level and category passes.
  [[nodiscard]] auto Accepts(Level level, Category category) const noexcept -> bool;

private:
  Level m_minimum;
  // Few categories are overridden in practice, so a flat list beats a map.
  std::vector<std::pair<Category, Level>> m_overrides;
};

}  // namespace flint::log
