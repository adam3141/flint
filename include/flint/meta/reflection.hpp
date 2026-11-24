#pragma once

// Detects C++26 static reflection (P2996). This is the only place that tests for it. See flint ADR-0003.
//
// Reflection code lives in flint/meta/detail/*_impl.hpp and is included only when FLINT_HAS_REFLECTION is 1. Public
// meta headers declare their function templates unconditionally, so callers are checked by every compiler.
//
// clang-based tools (clangd, clang-tidy) do not implement reflection yet and need definitions for constexpr templates,
// so they define FLINT_REFLECTION_TOOLING_STUBS (in .clangd and the clang-tidy command line) to get stub bodies from
// flint/meta/detail/*_stub.hpp. A real build without reflection and without that macro fails here, so stubs can never
// reach a binary.
#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): must be usable in #if
#define FLINT_HAS_REFLECTION 1
#elifdef FLINT_REFLECTION_TOOLING_STUBS
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): must be usable in #if
#define FLINT_HAS_REFLECTION 0
#else
#error "flint requires C++26 static reflection (GCC 16 or newer with -freflection)"
#endif
