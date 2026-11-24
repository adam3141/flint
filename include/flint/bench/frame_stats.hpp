#pragma once

#include <chrono>
#include <cstddef>
#include <vector>

#include "flint/bench/statistics.hpp"

namespace flint::bench {

/// A rolling window of frame times, for frame-rate overlays and for checking that a UI stays within its frame budget.
class FrameStats {
public:
  /// Four seconds of frames at 60 Hz.
  static constexpr std::size_t kDefaultCapacity = 240;

  /// @param capacity Frames kept in the window, for example 240 (four seconds at 60 Hz).
  explicit FrameStats(std::size_t capacity = kDefaultCapacity);

  /// Records one frame's duration, discarding the oldest when the window is full.
  auto AddFrame(std::chrono::nanoseconds duration) -> void;

  /// Returns statistics over the frames in the window.
  [[nodiscard]] auto GetSummary() const -> Summary;

  /// Returns the mean frames per second over the window, or 0 if empty.
  [[nodiscard]] auto FramesPerSecond() const -> double;

  /// Returns how many frames in the window took longer than `budget`, for example 16.67ms at 60 Hz.
  [[nodiscard]] auto CountOver(std::chrono::nanoseconds budget) const -> std::size_t;

  /// Returns the frames in the window, oldest first.
  [[nodiscard]] auto Frames() const -> std::vector<std::chrono::nanoseconds>;

  [[nodiscard]] auto Size() const noexcept -> std::size_t { return m_count; }

  auto Clear() noexcept -> void;

private:
  std::vector<std::chrono::nanoseconds> m_frames;
  std::size_t m_next{0};
  std::size_t m_count{0};
};

}  // namespace flint::bench
