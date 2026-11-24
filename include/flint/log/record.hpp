#pragma once

#include <chrono>
#include <cstdint>
#include <source_location>
#include <string>
#include <string_view>

#include "flint/log/category.hpp"
#include "flint/log/level.hpp"

namespace flint::log {

/// The source position a record was logged from.
///
/// The views normally point at the static strings of a std::source_location and are valid forever. Records rebuilt from
/// elsewhere (for example by flint-logcat from network input) must keep the strings alive for the life of the record.
struct SourcePosition {
  /// Path of the source file, as the compiler saw it.
  std::string_view file;
  /// Unmangled function signature, for example "void agate::Panel::Measure(agate::Size)".
  std::string_view function;
  std::uint32_t line{0};
  std::uint32_t column{0};

  /// Creates a source position from a std::source_location.
  [[nodiscard]] static auto From(const std::source_location& location) noexcept -> SourcePosition;
};

/// One log message and everything known about where and when it was produced.
struct Record {
  Level level{kInfo};
  Category category{kDefaultCategory};
  std::string message;
  SourcePosition source;
  /// Wall-clock time the record was created.
  std::chrono::system_clock::time_point timestamp;
  /// Time since the process's first Logger was constructed.
  std::chrono::nanoseconds applicationUptime{0};
  /// Time since the machine booted.
  std::chrono::nanoseconds systemUptime{0};
  std::uint64_t threadId{0};
  std::string threadName;
  /// Position of this record in the order its logger accepted records, starting at 1.
  std::uint64_t sequence{0};
};

/// Returns the function name without return type or parameters, for example "agate::Panel::Measure" for
/// "void agate::Panel::Measure(agate::Size)".
/// @param signature A function signature as produced by std::source_location::function_name.
[[nodiscard]] auto ShortFunctionName(std::string_view signature) -> std::string_view;

/// Returns the last component of a path, for example "panel.cpp" for "/src/agate/panel.cpp".
/// @param path A file path using '/' or '\\' separators.
[[nodiscard]] auto FileNameOf(std::string_view path) -> std::string_view;

}  // namespace flint::log
