# Benchmarking and profiling

## Benchmarks

```cpp
#include "flint/bench/benchmark.hpp"

flint::bench::Suite suite{"layout"};
suite.Add("measure 1000 buttons", [&] {
  for (auto& button : buttons) {
    flint::bench::DoNotOptimize(button.Measure(available));
  }
});
std::print("{}", flint::bench::FormatTable(suite.RunAll(/*filter=*/"")));
```

Each iteration is timed separately: after `warmupIterations` untimed runs, it runs for at least `minIterations` and `minTime`, and at most `maxIterations`. The result has min, max, mean, median, p90, p95, p99, standard deviation and total. Batch very small operations inside the function, as above.

`flint::bench::Run(name, function, options)` runs one benchmark. `FormatDuration` prints `850ns`, `12.30us`, `4.00ms`.

## Frame statistics

```cpp
#include "flint/bench/frame_stats.hpp"
#include "flint/bench/stopwatch.hpp"

flint::bench::FrameStats frames;            // last 240 frames
flint::bench::Stopwatch stopwatch;
while (running) {
  RenderFrame();
  frames.AddFrame(stopwatch.Restart());
}
frames.FramesPerSecond();
frames.GetSummary().p99;
frames.CountOver(std::chrono::microseconds{16'667});   // frames that missed 60 Hz
```

## Tracy profiling

Configure with the `profile` preset (`cmake --preset profile`), which sets `FLINT_ENABLE_TRACY=ON`. Then:

```cpp
#include "flint/profile/profile.hpp"

auto Panel::Measure(Size available) -> Size {
  FLINT_PROFILE_ZONE();                         // zone named after the function
  {
    FLINT_PROFILE_ZONE_NAMED("measure children");
    ...
  }
}

while (running) {
  ...
  FLINT_PROFILE_FRAME();                        // end of frame
  FLINT_PROFILE_PLOT("visible elements", count);
}
```

Run `tracy-profiler` in the dev container (it is built into the image), click **Connect**, and start the application. Add `{ "type": "tracy" }` to the logging configuration to see log records on Tracy's timeline.

Without `FLINT_ENABLE_TRACY` every macro compiles to nothing.
