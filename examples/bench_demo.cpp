// Demonstrates flint::bench by measuring the cost of the logger itself.
//
//   build/release/flint/examples/flint_bench_demo [filter]

#include <cstddef>
#include <exception>
#include <iostream>
#include <memory>
#include <print>
#include <source_location>
#include <span>
#include <string>
#include <string_view>

#include "flint/bench/benchmark.hpp"
#include "flint/json/parser.hpp"
#include "flint/log/callback_sink.hpp"
#include "flint/log/filter.hpp"
#include "flint/log/json_record.hpp"
#include "flint/log/level.hpp"
#include "flint/log/logger.hpp"
#include "flint/log/pattern_formatter.hpp"
#include "flint/log/record.hpp"

namespace {

constexpr int kBatch = 1000;

auto NullLogger(flint::log::Mode mode, flint::log::Level minimum) -> std::unique_ptr<flint::log::Logger> {
  auto logger = std::make_unique<flint::log::Logger>(flint::log::LoggerOptions{.mode = mode});
  logger->AddSink(std::make_unique<flint::log::CallbackSink>([](const flint::log::Record& record) { flint::bench::DoNotOptimize(record); }),
                  flint::log::Filter{minimum});
  return logger;
}

auto Run(std::span<char*> arguments) -> int {
  const std::string_view filter = arguments.size() > 1 ? std::string_view{arguments[1]} : std::string_view{};

  auto disabled = NullLogger(flint::log::Mode::Synchronous, flint::log::kWarning);
  auto synchronous = NullLogger(flint::log::Mode::Synchronous, flint::log::kTrace);
  auto asynchronous = NullLogger(flint::log::Mode::Asynchronous, flint::log::kTrace);

  flint::log::Record record;
  record.message = "a typical log message with a number 42 in it";
  record.source = flint::log::SourcePosition::From(std::source_location::current());
  auto formatter = flint::log::PatternFormatter::Create(flint::log::PatternFormatter::kFilePattern);

  flint::bench::Suite suite{"log"};
  suite.Add("disabled x1000", [&] {
    for (int i = 0; i < kBatch; ++i) {
      disabled->Debug("value {}", i);
    }
  });
  suite.Add("sync-null-sink x1000", [&] {
    for (int i = 0; i < kBatch; ++i) {
      synchronous->Info("value {}", i);
    }
  });
  suite.Add("async-enqueue x1000", [&] {
    for (int i = 0; i < kBatch; ++i) {
      asynchronous->Info("value {}", i);
    }
    asynchronous->Flush();
  });
  suite.Add("pattern-format x1000", [&] {
    std::string out;
    for (int i = 0; i < kBatch; ++i) {
      out.clear();
      formatter->Format(record, out);
    }
    flint::bench::DoNotOptimize(out);
  });
  suite.Add("json-record x1000", [&] {
    std::string out;
    for (int i = 0; i < kBatch; ++i) {
      out.clear();
      flint::log::AppendJsonRecord(record, out);
    }
    flint::bench::DoNotOptimize(out);
  });

  std::string jsonLine;
  flint::log::AppendJsonRecord(record, jsonLine);
  suite.Add("json-parse x1000", [&] {
    for (int i = 0; i < kBatch; ++i) {
      const auto parsed = flint::json::Parse(jsonLine);
      flint::bench::DoNotOptimize(parsed);
    }
  });

  const auto results = suite.RunAll(filter);
  std::print("{}", flint::bench::FormatTable(results));
  return 0;
}

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): this is the top-level handler; only reporting the failure itself could throw
auto main(int argc, char** argv) -> int {
  try {
    return Run(std::span{argv, static_cast<std::size_t>(argc)});
  } catch (const std::exception& exception) {
    // Top-level handler for truly exceptional failures (coding-style.md section 13).
    std::println(std::cerr, "flint_bench_demo: fatal: {}", exception.what());
    return 1;
  } catch (...) {
    std::println(std::cerr, "flint_bench_demo: fatal: unknown exception");
    return 1;
  }
}
