# 0008. Profiling with Tracy behind `FLINT_PROFILE_*` macros

- Status: Accepted
- Date: 2026-11-24

## Context

The UI must stay fluid. The owner suggested Tracy for its live timeline, its zones and its log view. Tracy instruments code with macros, which record each zone's source location in a static object.

## Decision

- `flint/profile/profile.hpp` defines `FLINT_PROFILE_ZONE()`, `FLINT_PROFILE_ZONE_NAMED(name)`, `FLINT_PROFILE_FRAME()`, `FLINT_PROFILE_PLOT(name, value)` and others. They forward to Tracy when `FLINT_ENABLE_TRACY` is on and compile to nothing otherwise. This is one of the few places the guide's macro rule is relaxed: only a macro can create the static source-location object at zero per-call cost.
- Tracy is fetched only when enabled (the `profile` CMake preset), in on-demand mode, so data is collected only while a profiler is connected.
- `TracySink` forwards log records to Tracy's message timeline, coloured by level.
- The Tracy profiler GUI (`tracy-profiler`, v0.15.0) is built into the dev container image.
- `flint::bench` is a separate set of utilities (a benchmark runner, statistics and `FrameStats`) for repeatable measurements and in-app frame-time overlays.

## Consequences

- Profiling has zero cost in normal builds.
- Tracy's own version must match between the client (FetchContent tag) and the GUI (Dockerfile `TRACY_VERSION`). Both are v0.15.0 and must be upgraded together.
