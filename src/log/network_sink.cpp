#include "flint/log/network_sink.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

#include "flint/log/json_record.hpp"
#include "flint/log/record.hpp"
#include "flint/net/tcp_client.hpp"
#include "flint/platform/thread.hpp"

namespace flint::log {

struct NetworkSink::State {
  // Guards every member below; `changed` waits on it.
  mutable std::mutex mutex;
  std::condition_variable_any changed;
  std::deque<std::string> pending;
  std::uint64_t dropped{0};
  bool connected{false};
  // Number of lines written into `pending` and number fully sent; Flush waits for sent to catch up.
  std::uint64_t enqueued{0};
  std::uint64_t sent{0};
  // Lines dropped from the front of `pending` since the current batch was taken. Those lines were part of the batch.
  std::size_t droppedSinceBatch{0};
};

NetworkSink::NetworkSink(NetworkSinkOptions options) : m_options{std::move(options)}, m_state{std::make_unique<State>()} {
  m_thread = std::jthread{[this](const std::stop_token& stopToken) {
    platform::SetCurrentThreadName("flint.netlog");
    SendLoop(stopToken);
  }};
}

NetworkSink::~NetworkSink() {
  m_thread.request_stop();
  m_state->changed.notify_all();
  m_thread.join();
}

auto NetworkSink::Write(const Record& record) -> void {
  std::string line;
  AppendJsonRecord(record, line);
  line.push_back('\n');
  {
    const std::scoped_lock lock{m_state->mutex};
    if (m_state->pending.size() >= m_options.bufferCapacity) {
      m_state->pending.pop_front();
      ++m_state->dropped;
      ++m_state->sent;  // The dropped line will never be sent; keep Flush's arithmetic consistent.
      ++m_state->droppedSinceBatch;
    }
    m_state->pending.push_back(std::move(line));
    ++m_state->enqueued;
  }
  m_state->changed.notify_one();
}

auto NetworkSink::Flush() -> void {
  std::unique_lock lock{m_state->mutex};
  if (!m_state->connected) {
    return;  // Nothing will be sent until reconnection; don't stall the logger.
  }
  const std::uint64_t target = m_state->enqueued;
  m_state->changed.wait_for(lock, m_options.connectTimeout, [this, target] { return m_state->sent >= target || !m_state->connected; });
}

auto NetworkSink::DroppedCount() const -> std::uint64_t {
  const std::scoped_lock lock{m_state->mutex};
  return m_state->dropped;
}

auto NetworkSink::IsConnected() const -> bool {
  const std::scoped_lock lock{m_state->mutex};
  return m_state->connected;
}

auto NetworkSink::SendLoop(const std::stop_token& stopToken) -> void {
  std::optional<net::TcpClient> client;
  std::chrono::milliseconds backoff = m_options.reconnectDelay;
  std::string batch;

  while (!stopToken.stop_requested()) {
    if (!client.has_value()) {
      auto connected = net::TcpClient::Connect(m_options.host, m_options.port, m_options.connectTimeout);
      if (!connected.has_value()) {
        std::unique_lock lock{m_state->mutex};
        m_state->changed.wait_for(lock, stopToken, backoff, [] { return false; });
        backoff = std::min(backoff * 2, m_options.maxReconnectDelay);
        continue;
      }
      client = std::move(*connected);
      backoff = m_options.reconnectDelay;
      const std::scoped_lock lock{m_state->mutex};
      m_state->connected = true;
    }

    std::size_t lineCount = 0;
    {
      std::unique_lock lock{m_state->mutex};
      m_state->changed.wait(lock, stopToken, [this] { return !m_state->pending.empty(); });
      if (m_state->pending.empty()) {
        break;
      }
      // Send in batches; lines stay queued until sent so nothing is lost on a failed send.
      batch.clear();
      m_state->droppedSinceBatch = 0;
      for (const std::string& line : m_state->pending) {
        batch.append(line);
        ++lineCount;
        constexpr std::size_t kMaxBatchBytes = std::size_t{64} * 1024;
        if (batch.size() >= kMaxBatchBytes) {
          break;
        }
      }
    }

    if (const auto result = client->SendAll(batch); !result.has_value()) {
      client.reset();
      const std::scoped_lock lock{m_state->mutex};
      m_state->connected = false;
      m_state->changed.notify_all();
      continue;
    }
    {
      const std::scoped_lock lock{m_state->mutex};
      // Lines dropped from the front during the send were batch lines and are already gone and counted.
      const std::size_t remaining = lineCount > m_state->droppedSinceBatch ? lineCount - m_state->droppedSinceBatch : 0;
      const std::size_t sentLines = std::min(remaining, m_state->pending.size());
      m_state->pending.erase(m_state->pending.begin(), m_state->pending.begin() + static_cast<std::ptrdiff_t>(sentLines));
      m_state->sent += sentLines;
      m_state->droppedSinceBatch = 0;
    }
    m_state->changed.notify_all();
  }

  // Best effort: send whatever is left on shutdown if connected.
  if (client.has_value()) {
    const std::scoped_lock lock{m_state->mutex};
    for (const std::string& line : m_state->pending) {
      if (!client->SendAll(line).has_value()) {
        break;
      }
    }
  }
}

}  // namespace flint::log
