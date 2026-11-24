#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "flint/bench/statistics.hpp"

namespace flint::bench {

inline constexpr std::size_t kDefaultWarmupIterations = 3;
inline constexpr std::size_t kDefaultMinIterations = 10;
inline constexpr std::chrono::milliseconds kDefaultMinTime{200};
inline constexpr std::size_t kDefaultMaxIterations = 1'000'000;

/// How long and how often a benchmark runs.
struct BenchmarkOptions {
  /// Untimed runs before measuring, to warm caches and branch predictors.
  std::size_t warmupIterations{kDefaultWarmupIterations};
  /// Measure at least this many iterations...
  std::size_t minIterations{kDefaultMinIterations};
  /// ...and keep going until this much time has been spent measuring...
  std::chrono::milliseconds minTime{kDefaultMinTime};
  /// ...but never more than this many iterations.
  std::size_t maxIterations{kDefaultMaxIterations};
};

/// The timings of one benchmark.
struct BenchmarkResult {
  std::string name;
  Summary summary;
};

namespace detail {

/// Makes the compiler treat `pointer` as used, so the computation that produced it is not optimised away.
auto Escape(const void* pointer) noexcept -> void;

}  // namespace detail

/// Keeps a value alive as far as the optimiser is concerned. Call on benchmark results.
template <typename Value>
auto DoNotOptimize(const Value& value) noexcept -> void {
  detail::Escape(&value);
}

/// Times a function repeatedly. Each call is timed separately, so the function should do at least a microsecond of work;
/// batch smaller operations inside it.
/// @param name A label for reports.
/// @param function The code under test.
/// @param options Iteration and time limits.
auto Run(std::string_view name, const std::function<void()>& function, const BenchmarkOptions& options = {}) -> BenchmarkResult;

/// A named collection of benchmarks that run together and print a table.
class Suite {
public:
  explicit Suite(std::string name, BenchmarkOptions options = {});

  /// Adds a benchmark to the suite.
  auto Add(std::string name, std::function<void()> function) -> void;

  /// Runs every benchmark whose name contains `filter` (all when empty), in the order added.
  [[nodiscard]] auto RunAll(std::string_view filter = {}) const -> std::vector<BenchmarkResult>;

private:
  struct Entry {
    std::string name;
    std::function<void()> function;
  };

  std::string m_name;
  BenchmarkOptions m_options;
  std::vector<Entry> m_entries;
};

/// Formats results as an aligned text table with min, median, mean, p95, p99 and max columns.
[[nodiscard]] auto FormatTable(std::span<const BenchmarkResult> results) -> std::string;

/// Formats a duration with a unit chosen for readability: "850ns", "12.3us", "4.56ms", "1.23s".
[[nodiscard]] auto FormatDuration(std::chrono::nanoseconds duration) -> std::string;

}  // namespace flint::bench
