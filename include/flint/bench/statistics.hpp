#pragma once

#include <chrono>
#include <cstddef>
#include <span>

namespace flint::bench {

/// Summary statistics of a set of durations.
struct Summary {
  std::size_t count{0};
  std::chrono::nanoseconds min{0};
  std::chrono::nanoseconds max{0};
  std::chrono::nanoseconds mean{0};
  std::chrono::nanoseconds median{0};
  std::chrono::nanoseconds p90{0};
  std::chrono::nanoseconds p95{0};
  std::chrono::nanoseconds p99{0};
  std::chrono::nanoseconds standardDeviation{0};
  std::chrono::nanoseconds total{0};
};

/// Computes summary statistics. Percentiles use the nearest-rank method.
/// @param samples The durations; need not be sorted. An empty span gives a zero Summary.
[[nodiscard]] auto Summarize(std::span<const std::chrono::nanoseconds> samples) -> Summary;

}  // namespace flint::bench
