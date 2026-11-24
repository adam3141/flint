#pragma once

#include <compare>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace flint::log {

/// Terminal colour used when a level is printed by a colour-capable sink.
enum class Colour : std::uint8_t {
  Default,
  Black,
  Red,
  Green,
  Yellow,
  Blue,
  Magenta,
  Cyan,
  White,
  Gray,
  BrightRed,
  BrightGreen,
  BrightYellow,
  BrightBlue,
  BrightMagenta,
  BrightCyan,
  BrightWhite,
  // White text on a red background, for Fatal.
  Alarm,
};

/// The severity of a log record. Higher values are more severe.
///
/// The six standard levels are spaced 100 apart so that custom levels can be placed between them, for example a
/// "Verbose" level at 150 sits between Trace and Debug, and a "Notice" level at 350 sits between Info and Warning.
/// Custom levels are named with RegisterLevel, usually from a logging configuration file.
struct Level {
  std::uint16_t severity{0};

  friend constexpr auto operator<=>(const Level& left, const Level& right) = default;
};

inline constexpr Level kTrace{.severity = 100};
inline constexpr Level kDebug{.severity = 200};
inline constexpr Level kInfo{.severity = 300};
inline constexpr Level kWarning{.severity = 400};
inline constexpr Level kError{.severity = 500};
inline constexpr Level kFatal{.severity = 600};

/// A threshold that is above every level. A filter with this minimum accepts nothing.
inline constexpr Level kOff{.severity = std::numeric_limits<std::uint16_t>::max()};

/// A named level, as stored in the level registry.
struct LevelInfo {
  std::string name;
  Level level;
  Colour colour{Colour::Default};
};

/// Why a custom level could not be registered.
enum class LevelError : std::uint8_t {
  EmptyName,
  NameInUse,
  SeverityInUse,
  // Severity 0 and kOff are reserved.
  ReservedSeverity,
};

/// Registers a custom level. Registering the same name with the same severity and colour again succeeds and returns
/// the existing level, so configuration can be reloaded.
/// @param name A unique name, compared without regard to ASCII case.
/// @param severity A unique severity between 1 and 65534.
/// @param colour The colour used by colour-capable sinks.
/// @return The new level, or why it could not be registered.
[[nodiscard]] auto RegisterLevel(std::string_view name, std::uint16_t severity, Colour colour = Colour::Default)
    -> std::expected<Level, LevelError>;

/// Finds a standard or custom level by name, ignoring ASCII case. "Off" finds kOff.
/// @param name The level name.
[[nodiscard]] auto FindLevel(std::string_view name) -> std::optional<Level>;

/// Returns the name of a standard or registered level, or nothing for an unregistered severity. The view is valid for
/// the life of the process.
/// @param level The level.
[[nodiscard]] auto LevelName(Level level) -> std::optional<std::string_view>;

/// Returns the colour of a standard or registered level, or Colour::Default for an unregistered severity.
/// @param level The level.
[[nodiscard]] auto LevelColour(Level level) -> Colour;

/// Returns every standard and registered level, ordered by severity.
[[nodiscard]] auto RegisteredLevels() -> std::vector<LevelInfo>;

/// Returns a human-readable description of a level error.
[[nodiscard]] auto Describe(LevelError error) -> std::string_view;

}  // namespace flint::log
