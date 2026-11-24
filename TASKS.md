# flint tasks

Status: `[ ]` to do, `[~]` in progress, `[x]` done. Newest work at the top of each section.

## Done

- [x] Build system: CMake 3.28+, C++26, GCC 16 with `-freflection`, the workspace warning set with `-Werror`, sanitizers, `_GLIBCXX_ASSERTIONS` in debug, FetchContent dependencies (ADR-0001, ADR-0002)
- [x] `flint::meta` enum reflection with tooling stubs (ADR-0003)
- [x] `flint::json`: value, parser (strict or config mode), writer (ADR-0005)
- [x] `flint::log`: levels plus custom levels, categories, filters, async and sync logger, Fatal flushes synchronously (ADR-0004)
- [x] Sinks: console (ANSI), file (rotation), JSON Lines (rotation), network (TCP JSON Lines, ADR-0007), Tracy, callback
- [x] Logging configuration from JSON with JSON Pointer errors (ADR-0006)
- [x] `flint-logcat` network collector
- [x] `flint::bench` (runner, statistics, FrameStats) and `FLINT_PROFILE_*` Tracy macros (ADR-0008)
- [x] `flint::platform` (uptime, threads, terminal, process) and `flint::net` (POSIX TCP)
- [x] 64 unit tests and a header self-containment check; clang-tidy and clang-format clean
- [x] Demos: `flint_log_demo`, `flint_bench_demo`

## Next

- [ ] Struct reflection in `flint::meta` (`ForEachMember`, member names and types), needed by agate view models
- [ ] Reflection-based JSON (de)serialisation (`json::ToJson`, `json::FromJson<T>`)
- [ ] An observable property and signal primitive (`flint::Signal<Args...>`) if agate's design puts change notification in flint. Waiting on the agate design questions
- [ ] Cache local-time conversion in `PatternFormatter` (it dominates the 0.9 µs format cost)
- [ ] Optional lock-free MPSC queue for the logger, if benchmarks under UI load show contention
- [ ] Hot reload of the logging configuration (watch the file)

## Portability

- [ ] Winsock implementation of `src/net/socket_posix.cpp` (Windows currently returns `NotSupported`)
- [ ] Build and test on Windows (MinGW-w64 GCC 16) and macOS (Homebrew GCC 16)
- [ ] Shared-library build: export macro (`FLINT_API`) and symbol visibility

## Questions for the owner

- [ ] Should logging at Fatal optionally terminate the process (for example a `LoggerOptions::abortOnFatal`)? Today it only flushes.
- [ ] Test code disables `readability-magic-numbers` and `bugprone-unchecked-optional-access` (`tests/.clang-tidy`, with reasons). Is that acceptable under coding-style.md section 19?
