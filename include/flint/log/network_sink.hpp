#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include "flint/log/record.hpp"
#include "flint/log/sink_interface.hpp"

namespace flint::log {

/// The port flint-logcat listens on by default.
inline constexpr std::uint16_t kDefaultLogPort = 9020;

/// Options for a NetworkSink.
struct NetworkSinkOptions {
  static constexpr std::size_t kDefaultBufferCapacity = 10'000;
  static constexpr std::chrono::milliseconds kDefaultReconnectDelay{250};
  static constexpr std::chrono::milliseconds kDefaultMaxReconnectDelay{5000};
  static constexpr std::chrono::milliseconds kDefaultConnectTimeout{1000};

  std::string host{"127.0.0.1"};
  std::uint16_t port{kDefaultLogPort};
  /// Lines kept while disconnected. When full, the oldest lines are dropped.
  std::size_t bufferCapacity{kDefaultBufferCapacity};
  /// Delay before the first reconnection attempt. It doubles after each failure, up to maxReconnectDelay.
  std::chrono::milliseconds reconnectDelay{kDefaultReconnectDelay};
  std::chrono::milliseconds maxReconnectDelay{kDefaultMaxReconnectDelay};
  std::chrono::milliseconds connectTimeout{kDefaultConnectTimeout};
};

/// Streams records over TCP as JSON Lines (see AppendJsonRecord), for a collector such as flint-logcat, netcat,
/// Vector or Logstash.
///
/// Sending happens on the sink's own thread, so a slow or absent collector never blocks the logger. While disconnected,
/// lines are buffered up to a limit and the sink reconnects with exponential back-off.
class NetworkSink final : public SinkInterface {
public:
  explicit NetworkSink(NetworkSinkOptions options);
  ~NetworkSink() override;

  NetworkSink(const NetworkSink&) = delete;
  auto operator=(const NetworkSink&) -> NetworkSink& = delete;
  NetworkSink(NetworkSink&&) = delete;
  auto operator=(NetworkSink&&) -> NetworkSink& = delete;

  auto Write(const Record& record) -> void override;

  /// Waits briefly (up to the connect timeout) for buffered lines to be sent. Never blocks indefinitely.
  auto Flush() -> void override;

  /// Returns how many lines were dropped because the buffer was full.
  [[nodiscard]] auto DroppedCount() const -> std::uint64_t;

  /// Reports whether the sink currently has a connection.
  [[nodiscard]] auto IsConnected() const -> bool;

private:
  struct State;

  auto SendLoop(const std::stop_token& stopToken) -> void;

  NetworkSinkOptions m_options;
  std::unique_ptr<State> m_state;
  std::jthread m_thread;
};

}  // namespace flint::log
