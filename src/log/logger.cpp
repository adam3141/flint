#include "flint/log/logger.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
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
#include "flint/log/level.hpp"
#include "flint/log/record.hpp"
#include "flint/log/sink_interface.hpp"
#include "flint/platform/thread.hpp"
#include "flint/platform/uptime.hpp"

namespace flint::log {

Logger::Logger(LoggerOptions options) : m_options{std::move(options)}, m_sinks{std::make_shared<const SinkList>()} {
  // Starts the application uptime clock if this is the first logger.
  static_cast<void>(ApplicationEpoch());

  for (auto& threshold : m_thresholds) {
    threshold.store(kOff.severity, std::memory_order_relaxed);
  }
  m_options.queueCapacity = std::max<std::size_t>(m_options.queueCapacity, 1);

  if (m_options.mode == Mode::Asynchronous) {
    m_queue.reserve(m_options.queueCapacity);
    m_worker = std::jthread{[this](const std::stop_token& stopToken) {
      platform::SetCurrentThreadName(m_options.threadName);
      WorkerLoop(stopToken);
    }};
  }
}

Logger::~Logger() {
  if (m_worker.joinable()) {
    // The worker drains the queue before it exits.
    m_worker.request_stop();
    m_worker.join();
  }
  std::shared_ptr<const SinkList> sinks;
  {
    const std::scoped_lock lock{m_sinksMutex};
    sinks = m_sinks;
  }
  FlushSinks(*sinks);
}

auto Logger::ApplicationEpoch() noexcept -> std::chrono::steady_clock::time_point {
  static const auto kEpoch = std::chrono::steady_clock::now();
  return kEpoch;
}

auto Logger::AddSink(std::unique_ptr<SinkInterface> sink, Filter filter) -> SinkId {
  const std::scoped_lock lock{m_sinksMutex};
  SinkList sinks = *m_sinks;
  const SinkId id{.value = m_nextSinkId++};
  sinks.push_back(SinkEntry{.id = id, .sink = std::shared_ptr<SinkInterface>{std::move(sink)}, .filter = std::move(filter)});
  PublishSinks(std::move(sinks));
  return id;
}

auto Logger::RemoveSink(SinkId id) -> bool {
  Flush();
  const std::scoped_lock lock{m_sinksMutex};
  SinkList sinks = *m_sinks;
  if (std::erase_if(sinks, [id](const SinkEntry& entry) { return entry.id == id; }) == 0) {
    return false;
  }
  PublishSinks(std::move(sinks));
  return true;
}

auto Logger::RemoveAllSinks() -> void {
  Flush();
  const std::scoped_lock lock{m_sinksMutex};
  PublishSinks(SinkList{});
}

auto Logger::SetSinkFilter(SinkId id, Filter filter) -> bool {
  const std::scoped_lock lock{m_sinksMutex};
  SinkList sinks = *m_sinks;
  const auto found = std::ranges::find(sinks, id, &SinkEntry::id);
  if (found == sinks.end()) {
    return false;
  }
  found->filter = std::move(filter);
  PublishSinks(std::move(sinks));
  return true;
}

// Called with m_sinksMutex held.
auto Logger::PublishSinks(SinkList sinks) -> void {
  RecomputeThresholds(sinks);
  m_sinks = std::make_shared<const SinkList>(std::move(sinks));
}

auto Logger::RecomputeThresholds(const SinkList& sinks) -> void {
  for (std::size_t id = 0; id < kMaxCategories; ++id) {
    const Category category{.id = static_cast<std::uint16_t>(id)};
    Level lowest = kOff;
    for (const SinkEntry& entry : sinks) {
      lowest = std::min(lowest, entry.filter.Threshold(category));
    }
    m_thresholds[id].store(lowest.severity, std::memory_order_relaxed);
  }
}

auto Logger::IsEnabled(Level level, Category category) const noexcept -> bool {
  if (category.id >= kMaxCategories) {
    return false;
  }
  return level.severity >= m_thresholds[category.id].load(std::memory_order_relaxed);
}

auto Logger::MakeRecord(Level level, Category category, std::string message, std::source_location location) -> Record {
  return Record{
      .level = level,
      .category = category,
      .message = std::move(message),
      .source = SourcePosition::From(location),
      .timestamp = std::chrono::system_clock::now(),
      .applicationUptime = std::chrono::steady_clock::now() - ApplicationEpoch(),
      .systemUptime = platform::SystemUptime().value_or(std::chrono::nanoseconds{0}),
      .threadId = platform::CurrentThreadId(),
      .threadName = platform::CurrentThreadName(),
      .sequence = 0,
  };
}

auto Logger::Submit(Level level, Category category, std::string message, std::source_location location) -> void {
  if (!IsEnabled(level, category)) {
    return;
  }
  Record record = MakeRecord(level, category, std::move(message), location);

  if (m_options.mode == Mode::Synchronous) {
    std::shared_ptr<const SinkList> sinks;
    {
      const std::scoped_lock lock{m_sinksMutex};
      sinks = m_sinks;
    }
    const std::scoped_lock lock{m_writeMutex};
    record.sequence = m_nextSequence++;
    WriteToSinks(record, *sinks);
    if (level >= kFatal) {
      FlushSinks(*sinks);
    }
    return;
  }

  Enqueue(std::move(record));
  if (level >= kFatal) {
    Flush();
  }
}

auto Logger::Enqueue(Record record) -> void {
  {
    std::unique_lock lock{m_queueMutex};
    if (m_queue.size() >= m_options.queueCapacity) {
      if (m_options.overflow == OverflowPolicy::Drop) {
        m_dropped.fetch_add(1, std::memory_order_relaxed);
        return;
      }
      m_queueSpace.wait(lock, [this] { return m_queue.size() < m_options.queueCapacity; });
    }
    record.sequence = m_nextSequence++;
    m_queue.push_back(std::move(record));
  }
  m_queueReady.notify_one();
}

auto Logger::WriteToSinks(const Record& record, const SinkList& sinks) -> void {
  for (const SinkEntry& entry : sinks) {
    if (entry.filter.Accepts(record.level, record.category)) {
      entry.sink->Write(record);
    }
  }
}

auto Logger::FlushSinks(const SinkList& sinks) -> void {
  for (const SinkEntry& entry : sinks) {
    entry.sink->Flush();
  }
}

auto Logger::WorkerLoop(const std::stop_token& stopToken) -> void {
  std::vector<Record> batch;
  batch.reserve(m_options.queueCapacity);
  while (true) {
    std::uint64_t lastSequence = 0;
    {
      std::unique_lock lock{m_queueMutex};
      const bool hasWork = m_queueReady.wait(lock, stopToken, [this] { return !m_queue.empty() || m_flushRequested; });
      if (!hasWork) {
        return;  // Stop requested and nothing left to write.
      }
      batch.swap(m_queue);
      m_flushRequested = false;
      lastSequence = m_nextSequence - 1;
    }
    m_queueSpace.notify_all();

    std::shared_ptr<const SinkList> sinks;
    {
      const std::scoped_lock lock{m_sinksMutex};
      sinks = m_sinks;
    }
    for (const Record& record : batch) {
      WriteToSinks(record, *sinks);
    }
    batch.clear();
    // Flushing once per batch keeps files current when idle, and costs little under load because batches grow.
    FlushSinks(*sinks);

    {
      const std::scoped_lock lock{m_queueMutex};
      m_writtenSequence = lastSequence;
    }
    m_written.notify_all();
  }
}

auto Logger::WaitUntilWritten(std::uint64_t sequence) -> void {
  std::unique_lock lock{m_queueMutex};
  m_flushRequested = true;
  m_queueReady.notify_one();
  m_written.wait(lock, [this, sequence] { return m_writtenSequence >= sequence && !m_flushRequested; });
}

auto Logger::Flush() -> void {
  if (m_options.mode == Mode::Synchronous) {
    std::shared_ptr<const SinkList> sinks;
    {
      const std::scoped_lock lock{m_sinksMutex};
      sinks = m_sinks;
    }
    const std::scoped_lock lock{m_writeMutex};
    FlushSinks(*sinks);
    return;
  }
  std::uint64_t target = 0;
  {
    const std::scoped_lock lock{m_queueMutex};
    target = m_nextSequence - 1;
  }
  WaitUntilWritten(target);
}

auto Logger::DroppedCount() const noexcept -> std::uint64_t {
  return m_dropped.load(std::memory_order_relaxed);
}

}  // namespace flint::log
