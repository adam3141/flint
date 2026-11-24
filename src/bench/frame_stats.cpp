#include "flint/bench/frame_stats.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <vector>

#include "flint/bench/statistics.hpp"

namespace flint::bench {

FrameStats::FrameStats(std::size_t capacity) : m_frames(std::max<std::size_t>(capacity, 1)) {}

auto FrameStats::AddFrame(std::chrono::nanoseconds duration) -> void {
  m_frames[m_next] = duration;
  m_next = (m_next + 1) % m_frames.size();
  m_count = std::min(m_count + 1, m_frames.size());
}

auto FrameStats::Frames() const -> std::vector<std::chrono::nanoseconds> {
  std::vector<std::chrono::nanoseconds> frames;
  frames.reserve(m_count);
  const std::size_t first = (m_next + m_frames.size() - m_count) % m_frames.size();
  for (std::size_t i = 0; i < m_count; ++i) {
    frames.push_back(m_frames[(first + i) % m_frames.size()]);
  }
  return frames;
}

auto FrameStats::GetSummary() const -> Summary {
  return Summarize(Frames());
}

auto FrameStats::FramesPerSecond() const -> double {
  if (m_count == 0) {
    return 0.0;
  }
  std::chrono::nanoseconds total{0};
  for (const auto frame : Frames()) {
    total += frame;
  }
  const double seconds = std::chrono::duration<double>{total}.count();
  return seconds > 0.0 ? static_cast<double>(m_count) / seconds : 0.0;
}

auto FrameStats::CountOver(std::chrono::nanoseconds budget) const -> std::size_t {
  const auto frames = Frames();
  return static_cast<std::size_t>(std::ranges::count_if(frames, [budget](auto frame) { return frame > budget; }));
}

auto FrameStats::Clear() noexcept -> void {
  m_next = 0;
  m_count = 0;
}

}  // namespace flint::bench
