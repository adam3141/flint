// Demonstrates flint logging: levels, custom levels, categories, sinks and configuration.
//
//   build/debug/flint/examples/flint_log_demo [config.json]
//
// Writes coloured output to the terminal, logs/demo.log (text), logs/demo.jsonl (JSON Lines), and streams to
// flint-logcat on port 9020 if it is running.

#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <print>
#include <span>
#include <thread>

#include "flint/log/config.hpp"
#include "flint/log/level.hpp"
#include "flint/log/log.hpp"
#include "flint/platform/thread.hpp"

namespace {

namespace log = flint::log;

const log::Channel kLayoutLog{"layout"};
const log::Channel kRenderLog{"render"};
const log::Channel kBindingLog{"binding"};

struct Size {
  int width{0};
  int height{0};
};

auto MeasurePanel(Size available) -> Size {
  kLayoutLog.Debug("measuring panel within {}x{}", available.width, available.height);
  constexpr int kPadding = 8;
  const Size desired{.width = available.width - (2 * kPadding), .height = available.height / 2};
  kLayoutLog.Trace("desired size {}x{}", desired.width, desired.height);
  return desired;
}

auto RenderFrame(int frame) -> void {
  kRenderLog.Debug("frame {} submitted", frame);  // filtered out on the console by the config
  constexpr int kSlowFrame = 2;
  if (frame == kSlowFrame) {
    kRenderLog.Warning("frame {} took 21.4ms (budget 16.7ms)", frame);
  }
}

auto Run(std::span<char*> arguments) -> int {
  const std::filesystem::path configPath =
      arguments.size() > 1 ? std::filesystem::path{arguments[1]} : std::filesystem::path{arguments[0]}.parent_path() / "log_demo.json";

  log::Info("starting flint log demo");
  if (auto configured = log::ConfigureFromFile(log::DefaultLogger(), configPath); !configured.has_value()) {
    log::Error("could not load {}: {}", configPath.string(), configured.error().Describe());
    return 1;
  }
  log::Info("configuration loaded from {}", configPath.string());

  const auto verbose = log::FindLevel("Verbose").value_or(log::kTrace);
  const auto notice = log::FindLevel("Notice").value_or(log::kInfo);

  log::Trace("trace: only in the file and JSON sinks");
  log::Log(verbose, "verbose: a custom level between Trace and Debug");
  log::Debug("debug: diagnostic detail");
  log::Info("info: normal operation");
  log::Log(notice, "notice: a custom level between Info and Warning");
  log::Warning("warning: something looks wrong");
  log::Error("error: something failed");

  constexpr Size kWindowSize{.width = 1280, .height = 720};
  constexpr int kFrames = 3;
  MeasurePanel(kWindowSize);
  for (int frame = 0; frame < kFrames; ++frame) {
    RenderFrame(frame);
  }
  kBindingLog.Error("binding errors are switched off on the console, but still reach the files");

  std::jthread worker{[] {
    flint::platform::SetCurrentThreadName("loader");
    log::Info("hello from a worker thread");
  }};
  worker.join();

  log::Fatal("fatal: written and flushed before this call returns (the process is not terminated)");
  log::Info("done; see logs/demo.log and logs/demo.jsonl");
  return 0;
}

}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): this is the top-level handler; only reporting the failure itself could throw
auto main(int argc, char** argv) -> int {
  try {
    flint::platform::SetCurrentThreadName("main");
    return Run(std::span{argv, static_cast<std::size_t>(argc)});
  } catch (const std::exception& exception) {
    // Top-level handler for truly exceptional failures (coding-style.md section 13).
    std::println(std::cerr, "flint_log_demo: fatal: {}", exception.what());
    return 1;
  } catch (...) {
    std::println(std::cerr, "flint_log_demo: fatal: unknown exception");
    return 1;
  }
}
