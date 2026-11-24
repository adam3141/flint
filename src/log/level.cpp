#include "flint/log/level.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
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

class LevelRegistry {
public:
  LevelRegistry() {
    m_levels.push_back(LevelInfo{.name = "Trace", .level = kTrace, .colour = Colour::Gray});
    m_levels.push_back(LevelInfo{.name = "Debug", .level = kDebug, .colour = Colour::Cyan});
    m_levels.push_back(LevelInfo{.name = "Info", .level = kInfo, .colour = Colour::Green});
    m_levels.push_back(LevelInfo{.name = "Warning", .level = kWarning, .colour = Colour::BrightYellow});
    m_levels.push_back(LevelInfo{.name = "Error", .level = kError, .colour = Colour::BrightRed});
    m_levels.push_back(LevelInfo{.name = "Fatal", .level = kFatal, .colour = Colour::Alarm});
  }

  auto Register(std::string_view name, std::uint16_t severity, Colour colour) -> std::expected<Level, LevelError> {
    if (name.empty()) {
      return std::unexpected(LevelError::EmptyName);
    }
    if (severity == 0 || severity == kOff.severity || text::EqualsIgnoreCase(name, "Off")) {
      return std::unexpected(LevelError::ReservedSeverity);
    }
    const std::scoped_lock lock{m_mutex};
    for (const LevelInfo& existing : m_levels) {
      const bool sameName = text::EqualsIgnoreCase(existing.name, name);
      const bool sameSeverity = existing.level.severity == severity;
      if (sameName && sameSeverity && existing.colour == colour) {
        return existing.level;
      }
      if (sameName) {
        return std::unexpected(LevelError::NameInUse);
      }
      if (sameSeverity) {
        return std::unexpected(LevelError::SeverityInUse);
      }
    }
    m_levels.push_back(LevelInfo{.name = std::string{name}, .level = Level{.severity = severity}, .colour = colour});
    return m_levels.back().level;
  }

  auto Find(std::string_view name) const -> std::optional<Level> {
    if (text::EqualsIgnoreCase(name, "Off")) {
      return kOff;
    }
    const std::shared_lock lock{m_mutex};
    const auto found = std::ranges::find_if(m_levels, [name](const LevelInfo& info) { return text::EqualsIgnoreCase(info.name, name); });
    return found == m_levels.end() ? std::nullopt : std::optional{found->level};
  }

  auto Lookup(Level level) const -> const LevelInfo* {
    const std::shared_lock lock{m_mutex};
    const auto found = std::ranges::find(m_levels, level, &LevelInfo::level);
    // Entries are never removed and std::deque never relocates them, so the pointer stays valid without the lock.
    return found == m_levels.end() ? nullptr : &*found;
  }

  auto Snapshot() const -> std::vector<LevelInfo> {
    const std::shared_lock lock{m_mutex};
    std::vector<LevelInfo> levels{m_levels.begin(), m_levels.end()};
    std::ranges::sort(levels, {}, &LevelInfo::level);
    return levels;
  }

private:
  // Guards m_levels.
  mutable std::shared_mutex m_mutex;
  std::deque<LevelInfo> m_levels;
};

auto Registry() -> LevelRegistry& {
  static LevelRegistry registry;
  return registry;
}

// The standard levels are looked up on every record, so answer them without taking the registry lock.
auto StandardIndex(Level level) -> std::optional<std::size_t> {
  constexpr std::uint16_t kSpacing = kDebug.severity - kTrace.severity;
  if (level.severity % kSpacing != 0 || level < kTrace || level > kFatal) {
    return std::nullopt;
  }
  return static_cast<std::size_t>((level.severity / kSpacing) - 1);
}

}  // namespace

auto RegisterLevel(std::string_view name, std::uint16_t severity, Colour colour) -> std::expected<Level, LevelError> {
  return Registry().Register(name, severity, colour);
}

auto FindLevel(std::string_view name) -> std::optional<Level> {
  return Registry().Find(name);
}

auto LevelName(Level level) -> std::optional<std::string_view> {
  static constexpr std::array<std::string_view, 6> kStandardNames{"Trace", "Debug", "Info", "Warning", "Error", "Fatal"};
  if (const auto index = StandardIndex(level); index.has_value()) {
    return kStandardNames.at(*index);
  }
  if (level == kOff) {
    return "Off";
  }
  const LevelInfo* info = Registry().Lookup(level);
  return info == nullptr ? std::nullopt : std::optional<std::string_view>{info->name};
}

auto LevelColour(Level level) -> Colour {
  static constexpr std::array<Colour, 6> kStandardColours{
      Colour::Gray,
      Colour::Cyan,
      Colour::Green,
      Colour::BrightYellow,
      Colour::BrightRed,
      Colour::Alarm,
  };
  if (const auto index = StandardIndex(level); index.has_value()) {
    return kStandardColours.at(*index);
  }
  const LevelInfo* info = Registry().Lookup(level);
  return info == nullptr ? Colour::Default : info->colour;
}

auto RegisteredLevels() -> std::vector<LevelInfo> {
  return Registry().Snapshot();
}

auto Describe(LevelError error) -> std::string_view {
  switch (error) {
    case LevelError::EmptyName:
      return "level name is empty";
    case LevelError::NameInUse:
      return "a level with that name already exists with a different severity or colour";
    case LevelError::SeverityInUse:
      return "another level already has that severity";
    case LevelError::ReservedSeverity:
      return "severity 0, 65535 and the name 'Off' are reserved";
  }
  return "unknown level error";
}

}  // namespace flint::log
