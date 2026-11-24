# flint

The foundation library of the [silica](https://github.com/adam3141/silica) workspace: everything that is not UI-specific, shared by [agate](https://github.com/adam3141/agate) (UI) and [opal](https://github.com/adam3141/opal) (editor), and usable by command-line tools, services, other GUI applications and real-time code.

| Module | What it gives you |
|---|---|
| `flint/log` | A fast asynchronous logger. Standard and **custom levels**, **categories**, console (ANSI colour), text-file, JSON Lines, TCP and Tracy sinks, and JSON configuration files |
| `flint/json` | An ordered JSON value, a strict or config-mode parser with line/column errors, and a streaming writer |
| `flint/bench` | A benchmark runner, statistics (p50/p95/p99) and frame-time tracking |
| `flint/profile` | Tracy profiler zones (`FLINT_PROFILE_ZONE()`), compiled out unless enabled |
| `flint/meta` | C++26 reflection helpers (enum names, more coming) |
| `flint/platform`, `flint/net`, `flint/text` | System uptime, thread names, terminal detection, TCP client and listener, number parsing |

```cpp
#include "flint/log/log.hpp"

const flint::log::Channel kLayoutLog{"layout"};

auto main() -> int {
  flint::log::Info("hello from {}", "flint");
  kLayoutLog.Debug("measured {} children", 42);
}
```

## Building

flint needs **GCC 16** (C++26 static reflection) and CMake 3.28 or newer. The silica dev container has everything set up.

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug   # from the silica root
```

Presets: `debug`, `release`, `profile` (Tracy on) and `asan` (AddressSanitizer and UBSan).

## Try it

```bash
build/debug/flint/tools/flint-logcat &               # network log viewer
build/debug/flint/examples/flint_log_demo            # console, files, JSON and network
build/release/flint/examples/flint_bench_demo        # logger benchmarks
```

## Documentation

- [Architecture](docs/architecture.md)
- [API guide](docs/api/README.md)
- [Decision records](docs/adr/README.md)
- [Tasks](TASKS.md)
- [Coding style](https://github.com/adam3141/silica/blob/main/docs/architecture/coding-style.md)

## Licence

MIT. See [LICENSE](LICENSE).
