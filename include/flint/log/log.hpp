#pragma once

// Convenience header: the process-wide default logger, free logging functions and channels.
//
//   flint::log::Info("Window opened at {}x{}", width, height);
//
//   inline const flint::log::Channel kLayoutLog{"layout"};
//   kLayoutLog.Debug("Measured {} children", count);

#include <format>
#include <string_view>
#include <utility>

#include "flint/log/category.hpp"
#include "flint/log/format_string.hpp"
#include "flint/log/level.hpp"
#include "flint/log/logger.hpp"

namespace flint::log {

/// Returns the process-wide logger used by the free logging functions and by Channel.
///
/// It starts with one console sink that writes kInfo and above to standard error. Reconfigure it with
/// ConfigureFromFile, or with AddSink and RemoveAllSinks.
[[nodiscard]] auto DefaultLogger() -> Logger&;

/// A category bound to a logger. Cheap to copy. Typically declared once per subsystem.
class Channel {
public:
  /// Registers (or finds) a category on the default logger. If the category table is full, the channel logs to
  /// kDefaultCategory. noexcept so channels can be namespace-scope constants: the only other failure is running out of
  /// memory, which terminates.
  /// @param name The category name.
  explicit Channel(std::string_view name) noexcept;

  /// Binds an existing category to a logger.
  /// @param logger The logger. It must outlive the channel.
  /// @param category The category.
  Channel(Logger& logger, Category category) noexcept;

  [[nodiscard]] auto GetCategory() const noexcept -> Category { return m_category; }

  [[nodiscard]] auto GetLogger() const noexcept -> Logger& { return *m_logger; }

  [[nodiscard]] auto IsEnabled(Level level) const noexcept -> bool { return m_logger->IsEnabled(level, m_category); }

  template <typename... Args>
  auto Log(Level level, FormatString<Args...> format, Args&&... args) const -> void {
    m_logger->Log<Args...>(level, m_category, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Trace(FormatString<Args...> format, Args&&... args) const -> void {
    Log<Args...>(kTrace, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Debug(FormatString<Args...> format, Args&&... args) const -> void {
    Log<Args...>(kDebug, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Info(FormatString<Args...> format, Args&&... args) const -> void {
    Log<Args...>(kInfo, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Warning(FormatString<Args...> format, Args&&... args) const -> void {
    Log<Args...>(kWarning, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Error(FormatString<Args...> format, Args&&... args) const -> void {
    Log<Args...>(kError, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Fatal(FormatString<Args...> format, Args&&... args) const -> void {
    Log<Args...>(kFatal, format, std::forward<Args>(args)...);
  }

private:
  Logger* m_logger{nullptr};
  Category m_category{kDefaultCategory};
};

/// Logs to the default logger at any standard or custom level, in the default category.
template <typename... Args>
auto Log(Level level, FormatString<Args...> format, Args&&... args) -> void {
  DefaultLogger().Log<Args...>(level, kDefaultCategory, format, std::forward<Args>(args)...);
}

template <typename... Args>
auto Trace(FormatString<Args...> format, Args&&... args) -> void {
  DefaultLogger().Log<Args...>(kTrace, kDefaultCategory, format, std::forward<Args>(args)...);
}

template <typename... Args>
auto Debug(FormatString<Args...> format, Args&&... args) -> void {
  DefaultLogger().Log<Args...>(kDebug, kDefaultCategory, format, std::forward<Args>(args)...);
}

template <typename... Args>
auto Info(FormatString<Args...> format, Args&&... args) -> void {
  DefaultLogger().Log<Args...>(kInfo, kDefaultCategory, format, std::forward<Args>(args)...);
}

template <typename... Args>
auto Warning(FormatString<Args...> format, Args&&... args) -> void {
  DefaultLogger().Log<Args...>(kWarning, kDefaultCategory, format, std::forward<Args>(args)...);
}

template <typename... Args>
auto Error(FormatString<Args...> format, Args&&... args) -> void {
  DefaultLogger().Log<Args...>(kError, kDefaultCategory, format, std::forward<Args>(args)...);
}

template <typename... Args>
auto Fatal(FormatString<Args...> format, Args&&... args) -> void {
  DefaultLogger().Log<Args...>(kFatal, kDefaultCategory, format, std::forward<Args>(args)...);
}

}  // namespace flint::log
