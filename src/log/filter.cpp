#include "flint/log/filter.hpp"

#include <algorithm>
#include <utility>

#include "flint/log/category.hpp"
#include "flint/log/level.hpp"

namespace flint::log {

Filter::Filter(Level minimum) noexcept : m_minimum{minimum} {}

auto Filter::SetMinimum(Level minimum) noexcept -> void {
  m_minimum = minimum;
}

auto Filter::GetMinimum() const noexcept -> Level {
  return m_minimum;
}

auto Filter::SetCategoryMinimum(Category category, Level minimum) -> void {
  const auto found = std::ranges::find(m_overrides, category, &std::pair<Category, Level>::first);
  if (found != m_overrides.end()) {
    found->second = minimum;
    return;
  }
  m_overrides.emplace_back(category, minimum);
}

auto Filter::ClearCategoryMinimum(Category category) -> void {
  std::erase_if(m_overrides, [category](const auto& entry) { return entry.first == category; });
}

auto Filter::Threshold(Category category) const noexcept -> Level {
  const auto found = std::ranges::find(m_overrides, category, &std::pair<Category, Level>::first);
  return found == m_overrides.end() ? m_minimum : found->second;
}

auto Filter::LowestThreshold() const noexcept -> Level {
  Level lowest = m_minimum;
  for (const auto& [category, minimum] : m_overrides) {
    lowest = std::min(lowest, minimum);
  }
  return lowest;
}

auto Filter::Accepts(Level level, Category category) const noexcept -> bool {
  return level >= Threshold(category);
}

}  // namespace flint::log
