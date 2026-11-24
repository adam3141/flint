#pragma once

// Profiling zones for the Tracy profiler (https://github.com/wolfpld/tracy).
//
// With FLINT_ENABLE_TRACY (CMake option, on in the `profile` preset) these forward to Tracy; otherwise they compile to
// nothing. Run `tracy-profiler` in the dev container and connect to the running application to see zones, frames,
// plots and log messages (via TracySink) live.
//
// These are macros because Tracy records each zone's source location in a static object at the point of use, which
// only a macro can create without a per-call cost.
//
//   auto Panel::Measure(Size available) -> Size {
//     FLINT_PROFILE_ZONE();
//     ...
//   }

// NOLINTBEGIN(cppcoreguidelines-macro-usage): Tracy zones need a static source location per use site (ADR-0008)
#ifdef FLINT_ENABLE_TRACY

#include <tracy/Tracy.hpp>

/// Profiles the enclosing scope, named after the function.
#define FLINT_PROFILE_ZONE() ZoneScoped
/// Profiles the enclosing scope with an explicit name (a string literal).
#define FLINT_PROFILE_ZONE_NAMED(name) ZoneScopedN(name)
/// Profiles the enclosing scope with a name and a 0xRRGGBB colour.
#define FLINT_PROFILE_ZONE_COLOURED(name, colour) ZoneScopedNC(name, colour)
/// Marks the end of a frame on the main frame timeline.
#define FLINT_PROFILE_FRAME() FrameMark
/// Marks the end of a frame on a named secondary timeline (a string literal).
#define FLINT_PROFILE_FRAME_NAMED(name) FrameMarkNamed(name)
/// Records a value on a named plot (a string literal).
#define FLINT_PROFILE_PLOT(name, value) TracyPlot(name, value)
/// Names the calling thread in the profiler (a string literal).
#define FLINT_PROFILE_THREAD_NAME(name) tracy::SetThreadName(name)

#else

#define FLINT_PROFILE_ZONE() static_cast<void>(0)
#define FLINT_PROFILE_ZONE_NAMED(name) static_cast<void>(0)
#define FLINT_PROFILE_ZONE_COLOURED(name, colour) static_cast<void>(0)
#define FLINT_PROFILE_FRAME() static_cast<void>(0)
#define FLINT_PROFILE_FRAME_NAMED(name) static_cast<void>(0)
#define FLINT_PROFILE_PLOT(name, value) static_cast<void>(0)
#define FLINT_PROFILE_THREAD_NAME(name) static_cast<void>(0)

#endif
// NOLINTEND(cppcoreguidelines-macro-usage)
