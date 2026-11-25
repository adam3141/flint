#include "flint/log/category.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <expected>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

#include "flint/text/ascii.hpp"

namespace flint::log {
namespace {

class CategoryRegistry {
public:
  CategoryRegistry() { m_names.emplace_back("app"); }

  auto Register(std::string_view name) -> std::expected<Category, CategoryError> {
    if (name.empty()) {
      return std::unexpected(CategoryError::EmptyName);
    }
    const std::scoped_lock lock{m_mutex};
    if (const auto existing = FindLocked(name); existing.has_value()) {
      return *existing;
    }
    if (m_names.size() >= kMaxCategories) {
      return std::unexpected(CategoryError::TooManyCategories);
    }
    m_names.emplace_back(name);
    return Category{.id = static_cast<std::uint16_t>(m_names.size() - 1)};
  }

  auto Find(std::string_view name) const -> std::optional<Category> {
    const std::shared_lock lock{m_mutex};
    return FindLocked(name);
  }

  auto Name(Category category) const -> std::string_view {
    const std::shared_lock lock{m_mutex};
    // Entries are never removed and std::deque never relocates them, so the view stays valid without the lock.
    return category.id < m_names.size() ? std::string_view{m_names[category.id]} : std::string_view{"?"};
  }

  auto Snapshot() const -> std::vector<std::string> {
    const std::shared_lock lock{m_mutex};
    return {m_names.begin(), m_names.end()};
  }

private:
  auto FindLocked(std::string_view name) const -> std::optional<Category> {
    const auto found =
        std::ranges::find_if(m_names, [name](const std::string& existing) { return text::EqualsIgnoreCase(existing, name); });
    if (found == m_names.end()) {
      return std::nullopt;
    }
    return Category{.id = static_cast<std::uint16_t>(found - m_names.begin())};
  }

  // Guards m_names.
  mutable std::shared_mutex m_mutex;
  std::deque<std::string> m_names;
};

auto Registry() -> CategoryRegistry& {
  // Never destroyed: the logger's worker thread can still be formatting records while static objects are destroyed at
  // exit, and formatting reads names from here.
  static auto* const kRegistry = new CategoryRegistry;  // NOLINT(cppcoreguidelines-owning-memory): deliberately never freed
  return *kRegistry;
}

}  // namespace

auto RegisterCategory(std::string_view name) -> std::expected<Category, CategoryError> {
  return Registry().Register(name);
}

auto FindCategory(std::string_view name) -> std::optional<Category> {
  return Registry().Find(name);
}

auto CategoryName(Category category) -> std::string_view {
  return Registry().Name(category);
}

auto RegisteredCategories() -> std::vector<std::string> {
  return Registry().Snapshot();
}

auto Describe(CategoryError error) -> std::string_view {
  switch (error) {
    case CategoryError::EmptyName:
      return "category name is empty";
    case CategoryError::TooManyCategories:
      return "too many categories";
  }
  return "unknown category error";
}

}  // namespace flint::log
