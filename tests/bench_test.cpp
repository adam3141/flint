#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "flint/bench/benchmark.hpp"
#include "flint/bench/frame_stats.hpp"
#include "flint/bench/statistics.hpp"

namespace {

using std::chrono::milliseconds;
using std::chrono::nanoseconds;

TEST(StatisticsTest, SummarisesSamples) {
  std::vector<nanoseconds> samples;
  for (int i = 100; i >= 1; --i) {
    samples.emplace_back(i);
  }
  const auto summary = flint::bench::Summarise(samples);
  EXPECT_EQ(summary.count, 100U);
  EXPECT_EQ(summary.min, nanoseconds{1});
  EXPECT_EQ(summary.max, nanoseconds{100});
  EXPECT_EQ(summary.median, nanoseconds{50});
  EXPECT_EQ(summary.p95, nanoseconds{95});
  EXPECT_EQ(summary.p99, nanoseconds{99});
  EXPECT_EQ(summary.total, nanoseconds{5050});
}

TEST(StatisticsTest, EmptyInputGivesZeroSummary) {
  EXPECT_EQ(flint::bench::Summarise({}).count, 0U);
}

TEST(BenchmarkTest, RunsAtLeastTheMinimumIterations) {
  int calls = 0;
  const auto result =
      flint::bench::Run("count", [&calls] { ++calls; }, {.warmupIterations = 2, .minIterations = 5, .minTime = milliseconds{0}});
  EXPECT_EQ(result.summary.count, 5U);
  EXPECT_EQ(calls, 7);
}

TEST(BenchmarkTest, SuiteFiltersByName) {
  flint::bench::Suite suite{"suite", {.warmupIterations = 0, .minIterations = 1, .minTime = milliseconds{0}}};
  suite.Add("alpha", [] {});
  suite.Add("beta", [] {});
  const auto results = suite.RunAll("bet");
  ASSERT_EQ(results.size(), 1U);
  EXPECT_EQ(results[0].name, "suite/beta");
  EXPECT_NE(flint::bench::FormatTable(results).find("suite/beta"), std::string::npos);
}

TEST(FormatDurationTest, PicksReadableUnits) {
  EXPECT_EQ(flint::bench::FormatDuration(nanoseconds{850}), "850ns");
  EXPECT_EQ(flint::bench::FormatDuration(nanoseconds{12'300}), "12.30us");
  EXPECT_EQ(flint::bench::FormatDuration(milliseconds{4}), "4.00ms");
}

TEST(FrameStatsTest, KeepsARollingWindow) {
  flint::bench::FrameStats stats{3};
  stats.AddFrame(milliseconds{10});
  stats.AddFrame(milliseconds{20});
  stats.AddFrame(milliseconds{30});
  stats.AddFrame(milliseconds{40});
  EXPECT_EQ(stats.Size(), 3U);
  EXPECT_EQ(stats.Frames(), (std::vector<nanoseconds>{milliseconds{20}, milliseconds{30}, milliseconds{40}}));
  EXPECT_EQ(stats.CountOver(milliseconds{25}), 2U);
  EXPECT_NEAR(stats.FramesPerSecond(), 3.0 / 0.09, 1e-9);
}

}  // namespace
