#pragma once

#include <chrono>

namespace flint::bench {

/// Measures elapsed time on the steady clock. Starts running when constructed.
class Stopwatch {
public:
  using Clock = std::chrono::steady_clock;

  Stopwatch() noexcept : m_start{Clock::now()} {}

  /// Returns the time since construction or the last Restart.
  [[nodiscard]] auto Elapsed() const noexcept -> std::chrono::nanoseconds { return Clock::now() - m_start; }

  /// Returns the elapsed time and starts timing again from now.
  auto Restart() noexcept -> std::chrono::nanoseconds {
    const auto now = Clock::now();
    const auto elapsed = now - m_start;
    m_start = now;
    return elapsed;
  }

private:
  Clock::time_point m_start;
};

}  // namespace flint::bench
