#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <source_location>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "flint/log/category.hpp"
#include "flint/log/filter.hpp"
#include "flint/log/format_string.hpp"
#include "flint/log/level.hpp"
#include "flint/log/record.hpp"
#include "flint/log/sink_interface.hpp"

namespace flint::log {

/// Where sink writes happen.
enum class Mode : std::uint8_t {
  /// Records are queued and written by a background thread. Logging costs one format and one queue push.
  Asynchronous,
  /// Records are written by the logging thread before the call returns. Useful for tests and short-lived tools.
  Synchronous,
};

/// What an asynchronous logger does when its queue is full.
enum class OverflowPolicy : std::uint8_t {
  /// The logging thread waits for space. Nothing is lost.
  Block,
  /// The record is dropped and counted. The logging thread never waits.
  Drop,
};

/// Construction options for a Logger.
struct LoggerOptions {
  static constexpr std::size_t kDefaultQueueCapacity = 8192;

  Mode mode{Mode::Asynchronous};
  /// Records the asynchronous queue holds before the overflow policy applies.
  std::size_t queueCapacity{kDefaultQueueCapacity};
  OverflowPolicy overflow{OverflowPolicy::Block};
  /// Name of the background thread, as shown by debuggers and in log records.
  std::string threadName{"flint.log"};
};

/// Identifies a sink added to a logger.
struct SinkId {
  std::uint32_t value{0};

  friend constexpr auto operator<=>(const SinkId& left, const SinkId& right) = default;
};

/// Routes log records to sinks.
///
/// Each sink has its own Filter. A record is formatted only if at least one sink would accept it, so disabled levels and
/// categories cost one relaxed atomic load.
///
/// Records at kFatal or above are written and flushed before the logging call returns, in either mode. Logging at
/// kFatal does not terminate the process.
///
/// All member functions are thread-safe.
class Logger final {
public:
  explicit Logger(LoggerOptions options = {});
  ~Logger();

  Logger(const Logger&) = delete;
  auto operator=(const Logger&) -> Logger& = delete;
  Logger(Logger&&) = delete;
  auto operator=(Logger&&) -> Logger& = delete;

  /// Adds a sink.
  /// @param sink The sink. The logger owns it from now on.
  /// @param filter Which records the sink receives.
  /// @return An identifier for changing or removing the sink later.
  auto AddSink(std::unique_ptr<SinkInterface> sink, Filter filter = Filter{}) -> SinkId;

  /// Removes a sink after writing and flushing everything queued before the call.
  /// @return false if no sink has that identifier.
  auto RemoveSink(SinkId id) -> bool;

  /// Removes every sink after writing and flushing everything queued before the call.
  auto RemoveAllSinks() -> void;

  /// Replaces the filter of a sink.
  /// @return false if no sink has that identifier.
  auto SetSinkFilter(SinkId id, Filter filter) -> bool;

  /// Reports whether any sink would accept a record with this level and category.
  [[nodiscard]] auto IsEnabled(Level level, Category category) const noexcept -> bool;

  /// Logs a pre-formatted message. Prefer Log() and the level functions, which format only when enabled.
  /// @param level The record's level.
  /// @param category The record's category.
  /// @param message The message text.
  /// @param location The call site.
  auto Submit(Level level, Category category, std::string message, std::source_location location = std::source_location::current()) -> void;

  /// Formats and logs a message if any sink would accept it.
  template <typename... Args>
  auto Log(Level level, Category category, FormatString<Args...> format, Args&&... args) -> void {
    if (IsEnabled(level, category)) {
      Submit(level, category, std::format(format.format, std::forward<Args>(args)...), format.location);
    }
  }

  template <typename... Args>
  auto Trace(FormatString<Args...> format, Args&&... args) -> void {
    Log<Args...>(kTrace, kDefaultCategory, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Debug(FormatString<Args...> format, Args&&... args) -> void {
    Log<Args...>(kDebug, kDefaultCategory, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Info(FormatString<Args...> format, Args&&... args) -> void {
    Log<Args...>(kInfo, kDefaultCategory, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Warning(FormatString<Args...> format, Args&&... args) -> void {
    Log<Args...>(kWarning, kDefaultCategory, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Error(FormatString<Args...> format, Args&&... args) -> void {
    Log<Args...>(kError, kDefaultCategory, format, std::forward<Args>(args)...);
  }

  template <typename... Args>
  auto Fatal(FormatString<Args...> format, Args&&... args) -> void {
    Log<Args...>(kFatal, kDefaultCategory, format, std::forward<Args>(args)...);
  }

  /// Blocks until every record submitted before the call has been written and every sink flushed.
  auto Flush() -> void;

  /// Returns how many records were dropped because the queue was full (OverflowPolicy::Drop only).
  [[nodiscard]] auto DroppedCount() const noexcept -> std::uint64_t;

  /// Returns the time the first Logger in the process was constructed. Application uptime is measured from here.
  [[nodiscard]] static auto ApplicationEpoch() noexcept -> std::chrono::steady_clock::time_point;

private:
  struct SinkEntry {
    SinkId id;
    std::shared_ptr<SinkInterface> sink;
    Filter filter;
  };

  using SinkList = std::vector<SinkEntry>;

  static auto MakeRecord(Level level, Category category, std::string message, std::source_location location) -> Record;
  static auto WriteToSinks(const Record& record, const SinkList& sinks) -> void;
  static auto FlushSinks(const SinkList& sinks) -> void;
  auto PublishSinks(SinkList sinks) -> void;
  auto RecomputeThresholds(const SinkList& sinks) -> void;
  auto Enqueue(Record record) -> void;
  auto WorkerLoop(const std::stop_token& stopToken) -> void;
  auto WaitUntilWritten(std::uint64_t sequence) -> void;

  LoggerOptions m_options;

  // Lowest accepted severity per category over all sinks; indexed by Category::id.
  std::array<std::atomic<std::uint16_t>, kMaxCategories> m_thresholds{};

  // Guards m_sinks and m_nextSinkId. Readers take a snapshot of the list, so sinks are never called under this lock.
  mutable std::mutex m_sinksMutex;
  std::shared_ptr<const SinkList> m_sinks;
  std::uint32_t m_nextSinkId{1};

  // Serialises sink calls in synchronous mode.
  std::mutex m_writeMutex;

  // Guards m_queue, m_nextSequence, m_writtenSequence and m_flushRequested; the condition variables wait on it.
  std::mutex m_queueMutex;
  std::condition_variable_any m_queueReady;
  std::condition_variable m_queueSpace;
  std::condition_variable m_written;
  std::vector<Record> m_queue;
  std::uint64_t m_nextSequence{1};
  std::uint64_t m_writtenSequence{0};
  bool m_flushRequested{false};

  std::atomic<std::uint64_t> m_dropped{0};

  // Declared last so the worker stops before the members it uses are destroyed.
  std::jthread m_worker;
};

}  // namespace flint::log
