# 0001. C++26 with GCC 16 as the reference compiler

- Status: Accepted
- Date: 2026-11-24

## Context

agate's dependency-property and binding system benefits greatly from C++26 static reflection (P2996): property metadata, enum names, and view-model member discovery can be generated at compile time instead of being registered by hand or with macros. In November 2026 GCC 16 is the only production compiler with P2996 (`-freflection`). Clang 23 and MSVC do not implement it. Ubuntu 26.04 packages a GCC 16 snapshot (`16.0.1 20260322`), which compiles the reflection used here (`^^T`, `[: :]`, `template for`, `std::meta::enumerators_of`, `std::define_static_array`, `std::define_static_string`).

The workspace coding guide was written for C++23.

## Decision

- All silica libraries use **C++26** (`CMAKE_CXX_STANDARD 26`, no extensions).
- **GCC 16** with `-freflection` is the reference compiler. `flint_enable_reflection()` in `cmake/FlintCompilerOptions.cmake` adds the flag and rejects older GCC.
- The coding guide is updated from C++23 to C++26.

## Consequences

- Windows and macOS builds need a compiler with P2996 before they work: GCC 16 (MinGW-w64 or Homebrew) today, or Clang and MSVC once they implement it. The platform layer (`flint/platform`) is already written for all three, but only Linux is tested.
- clangd and clang-tidy cannot parse reflection. See ADR-0003 for how that is contained.
- The Ubuntu GCC 16 package is a pre-release snapshot. Compiler bugs are possible; workarounds must be commented with the GCC version.
