# flint architecture

flint is the foundation library of the silica workspace. It holds everything that is not UI-specific, so that it can serve agate (UI), opal (editor), command-line tools and services, other GUI applications, and real-time or game code. flint has no dependency on SDL, OpenGL or any windowing system.

## Design principles

- **Cheap when idle.** Disabled log statements, profiler zones in non-profile builds, and unused facilities cost nothing or nearly nothing.
- **Errors are values.** Fallible functions return `std::expected<T, E>` with a project error type. Exceptions are reserved for truly exceptional failures (coding-style.md section 13).
- **Portable by construction.** Operating-system code lives only in `src/platform` and `src/net`, behind small functions. All other code is standard C++26.
- **Reflection in one place.** C++26 reflection is used only in `flint/meta` (ADR-0003).

## Modules

```
flint/
  include/flint/
    platform/   OS facts: system uptime, thread id and name, terminal colour support, process id, host name
    text/       ASCII helpers and number parsing/formatting (<charconv> wrappers)
    meta/       C++26 reflection utilities (enum names today; property and struct reflection next)
    xml/        Element, Attribute, Location and Parse (pugixml inside)
    net/        TcpClient and TcpListener (POSIX; Windows to do)
    log/        Levels, categories, filters, Logger, sinks, pattern formatting, configuration
    bench/      Stopwatch, statistics, benchmark runner, FrameStats
    profile/    FLINT_PROFILE_* macros (Tracy)
  src/          Implementations; src/net/socket_posix.* is the private socket layer
  tools/logcat  flint-logcat: network log collector
  examples/     flint_log_demo, flint_bench_demo
  tests/        GoogleTest unit tests, plus a check that every header compiles on its own
```

Dependencies between modules (arrows point at what is used):

```
log ──▶ glaze, net, platform, meta, text
bench ─▶ (std only)
xml ───▶ pugixml (private), text
net ───▶ (OS sockets)
meta ──▶ text
profile ▶ Tracy (optional)
```

## Logging

See ADR-0004 for the rationale and [api/logging.md](api/logging.md) for usage.

```
caller thread                                   worker thread ("flint.log")
─────────────                                   ───────────────────────────
log::Info("x = {}", x)
  │  FormatString captures source_location (consteval, checked format)
  ├─ IsEnabled(level, category)? ── no ──▶ return   (one relaxed atomic load)
  ├─ std::format message
  ├─ build Record (time, uptimes, thread)
  └─ push to bounded queue ───────────────▶ swap queue out, then for each record:
                                               for each sink whose Filter accepts it: sink.Write
                                             flush every sink once per batch
```

- Sinks: `ConsoleSink` (ANSI colour, auto-detected, honours NO_COLOR and FORCE_COLOR), `FileSink` (pattern text with size rotation), `JsonFileSink` (JSON Lines with rotation), `NetworkSink` (TCP JSON Lines on its own thread), `TracySink`, and `CallbackSink`.
- Configuration (`ConfigureFromFile`) registers levels and categories, then builds every sink before it replaces the logger's sinks, so a bad file leaves the old configuration in place.
- Levels and categories live in process-wide registries. Names are stable string views for the life of the process.

## Threading

- `Logger` is fully thread-safe. One mutex guards the queue, and a separate one guards the sink list. The sink list is copied on write and read as a `shared_ptr` snapshot, so the worker never calls sinks while holding the logger's locks.
- A given sink is called from only one thread at a time.
- `NetworkSink` has its own thread. Its `Write` only appends to a bounded buffer.

## Performance baseline

Measured on 2026-11-24 with `flint_bench_demo` (RelWithDebInfo, GCC 16, RTX 4000 SFF host). Times are per call:

| Operation | Median |
|---|---|
| Disabled log statement | 4 ns |
| Log through a synchronous logger to a null sink | 155 ns |
| Log through an asynchronous logger (enqueue, amortised flush) | 0.5 µs |
| Pattern-format one record (local time) | 0.9 µs |
| Serialise one record to JSON | 0.46 µs |
| Parse one JSON log line | 1.4 µs |

Local-time conversion dominates pattern formatting. Caching it is in TASKS.md.

## Portability status

| Area | Linux | Windows | macOS |
|---|---|---|---|
| platform | tested | written, untested | written, untested |
| net | tested | stub (`NotSupported`) | POSIX path, untested |
| everything else | tested | standard C++ | standard C++ |

All platforms need a compiler with P2996 reflection (ADR-0001).
