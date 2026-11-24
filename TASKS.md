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
- [x] Unit tests (89) and a header self-containment check; clang-tidy and clang-format clean
- [x] Demos: `flint_log_demo`, `flint_bench_demo`
- [x] `flint::event::Signal` with scoped `Connection` and `ConnectionGroup`; `flint::model::Observable` and `Command` (ADR-0009); 25 tests

## Milestone 4 (first tasks; not started, the owner will start milestone 4)

Owner decision (2026-11-25): use established libraries instead of flint's own JSON and XML parsers.

- [ ] **Replace `flint::json` with glaze.** Check glaze builds cleanly with GCC 16, `-freflection` and the workspace warnings. Move the logger configuration, the JSON Lines and network sinks, and `flint-logcat` to it. Keep `std::expected` errors with line and column for configuration files. Drop the planned reflection-based JSON (de)serialisation task, which glaze covers. Supersede ADR-0005 with a new ADR.
- [ ] **Replace the hand-written XML parser with pugixml** (MIT). Keep `flint::xml::Element`, `Attribute` and `Location` as a thin wrapper, with line and column computed from pugixml's offsets, so agate's markup loader does not change. Keep rejecting DTDs and limiting nesting depth. Supersede ADR-0010 with a new ADR. flint#4 (the hand-written parser) is on hold until this is decided in code.

## Next

- [ ] Struct reflection in `flint::meta` (`ForEachMember`, member names and types), needed by agate view models
- [ ] ~~Reflection-based JSON (de)serialisation~~: covered by glaze (milestone 4 task above)
- [ ] Parameterised commands (`Command<T>` with a CommandParameter), when agate controls need them
- [ ] Cache local-time conversion in `PatternFormatter` (it dominates the 0.9 µs format cost)
- [ ] Optional lock-free MPSC queue for the logger, if benchmarks under UI load show contention
- [ ] Hot reload of the logging configuration (watch the file)

## Portability

- [ ] Winsock implementation of `src/net/socket_posix.cpp` (Windows currently returns `NotSupported`)
- [ ] Build and test on Windows (MinGW-w64 GCC 16) and macOS (Homebrew GCC 16)
- [ ] Shared-library build: export macro (`FLINT_API`) and symbol visibility

## Questions for the owner

- [x] Should logging at Fatal optionally terminate the process? **No** (owner, 2026-11-25): Fatal flushes and returns; stopping is the caller's decision (ADR-0004).
- [x] Test code disables `readability-magic-numbers` and `bugprone-unchecked-optional-access` (`tests/.clang-tidy`, with reasons). Accepted by the owner (2026-11-24).
