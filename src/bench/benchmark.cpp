#include "flint/bench/benchmark.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <format>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "flint/bench/statistics.hpp"
#include "flint/bench/stopwatch.hpp"

namespace flint::bench {
namespace detail {

namespace {
// Storing to an atomic in another translation unit is opaque to the optimiser without LTO, and cheap.
std::atomic<const void*> gEscaped{nullptr};  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables): optimisation barrier
}  // namespace

auto Escape(const void* pointer) noexcept -> void {
  gEscaped.store(pointer, std::memory_order_relaxed);
}

}  // namespace detail

auto Run(std::string_view name, const std::function<void()>& function, const BenchmarkOptions& options) -> BenchmarkResult {
  for (std::size_t i = 0; i < options.warmupIterations; ++i) {
    function();
  }
  std::vector<std::chrono::nanoseconds> samples;
  // Reserve for typical runs without committing memory for maxIterations up front.
  constexpr std::size_t kInitialSampleCapacity = 4096;
  samples.reserve(std::min(options.maxIterations, kInitialSampleCapacity));
  const Stopwatch total;
  while (samples.size() < options.maxIterations && (samples.size() < options.minIterations || total.Elapsed() < options.minTime)) {
    const Stopwatch stopwatch;
    function();
    samples.push_back(stopwatch.Elapsed());
  }
  return BenchmarkResult{.name = std::string{name}, .summary = Summarise(samples)};
}

Suite::Suite(std::string name, BenchmarkOptions options) : m_name{std::move(name)}, m_options{options} {}

auto Suite::Add(std::string name, std::function<void()> function) -> void {
  m_entries.push_back(Entry{.name = std::move(name), .function = std::move(function)});
}

auto Suite::RunAll(std::string_view filter) const -> std::vector<BenchmarkResult> {
  std::vector<BenchmarkResult> results;
  for (const Entry& entry : m_entries) {
    if (filter.empty() || entry.name.contains(filter)) {
      results.push_back(Run(std::format("{}/{}", m_name, entry.name), entry.function, m_options));
    }
  }
  return results;
}

auto FormatDuration(std::chrono::nanoseconds duration) -> std::string {
  constexpr double kThousand = 1000.0;
  const auto nanoseconds = static_cast<double>(duration.count());
  if (nanoseconds < kThousand) {
    return std::format("{}ns", duration.count());
  }
  if (nanoseconds < kThousand * kThousand) {
    return std::format("{:.2f}us", nanoseconds / kThousand);
  }
  if (nanoseconds < kThousand * kThousand * kThousand) {
    return std::format("{:.2f}ms", nanoseconds / (kThousand * kThousand));
  }
  return std::format("{:.2f}s", nanoseconds / (kThousand * kThousand * kThousand));
}

auto FormatTable(std::span<const BenchmarkResult> results) -> std::string {
  std::size_t nameWidth = std::string_view{"benchmark"}.size();
  for (const BenchmarkResult& result : results) {
    nameWidth = std::max(nameWidth, result.name.size());
  }
  const auto padded = [nameWidth](std::string_view name) {
    std::string text{name};
    text.resize(nameWidth, ' ');
    return text;
  };
  std::string table = std::format("{}  {:>10}  {:>10}  {:>10}  {:>10}  {:>10}  {:>10}  {:>10}\n",
                                  padded("benchmark"),
                                  "iterations",
                                  "min",
                                  "median",
                                  "mean",
                                  "p95",
                                  "p99",
                                  "max");
  for (const BenchmarkResult& result : results) {
    const Summary& summary = result.summary;
    table += std::format("{}  {:>10}  {:>10}  {:>10}  {:>10}  {:>10}  {:>10}  {:>10}\n",
                         padded(result.name),
                         summary.count,
                         FormatDuration(summary.min),
                         FormatDuration(summary.median),
                         FormatDuration(summary.mean),
                         FormatDuration(summary.p95),
                         FormatDuration(summary.p99),
                         FormatDuration(summary.max));
  }
  return table;
}

}  // namespace flint::bench
