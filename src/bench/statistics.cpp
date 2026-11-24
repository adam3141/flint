#include "flint/bench/statistics.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace flint::bench {
namespace {

constexpr double kP90 = 0.90;
constexpr double kP95 = 0.95;
constexpr double kP99 = 0.99;

// Nearest-rank percentile of sorted samples.
auto Percentile(std::span<const std::chrono::nanoseconds> sorted, double fraction) -> std::chrono::nanoseconds {
  const auto rank = static_cast<std::size_t>(std::ceil(fraction * static_cast<double>(sorted.size())));
  return sorted[std::clamp<std::size_t>(rank, 1, sorted.size()) - 1];
}

}  // namespace

auto Summarise(std::span<const std::chrono::nanoseconds> samples) -> Summary {
  if (samples.empty()) {
    return {};
  }
  std::vector<std::chrono::nanoseconds> sorted{samples.begin(), samples.end()};
  std::ranges::sort(sorted);

  std::chrono::nanoseconds total{0};
  for (const auto sample : sorted) {
    total += sample;
  }
  const auto count = static_cast<double>(sorted.size());
  const double mean = static_cast<double>(total.count()) / count;
  double squares = 0.0;
  for (const auto sample : sorted) {
    const double difference = static_cast<double>(sample.count()) - mean;
    squares += difference * difference;
  }

  return Summary{
      .count = sorted.size(),
      .min = sorted.front(),
      .max = sorted.back(),
      .mean = std::chrono::nanoseconds{static_cast<std::int64_t>(std::llround(mean))},
      .median = Percentile(sorted, 0.5),
      .p90 = Percentile(sorted, kP90),
      .p95 = Percentile(sorted, kP95),
      .p99 = Percentile(sorted, kP99),
      .standardDeviation = std::chrono::nanoseconds{static_cast<std::int64_t>(std::llround(std::sqrt(squares / count)))},
      .total = total,
  };
}

}  // namespace flint::bench
