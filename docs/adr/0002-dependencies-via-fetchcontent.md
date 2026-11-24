# 0002. Third-party dependencies via CMake FetchContent

- Status: Accepted
- Date: 2026-11-24

## Context

The libraries need a few third-party projects: GoogleTest, Tracy, and for agate SDL3 and text-rendering libraries. Options considered: vcpkg manifests, CMake FetchContent, and distribution packages. Cross-platform builds are a goal. The owner chose FetchContent.

## Decision

- Dependencies are declared in `cmake/FlintDependencies.cmake` (and agate's equivalent) with `FetchContent_Declare`, pinned to a **release tag**, `GIT_SHALLOW`, and `SYSTEM` so their headers do not trigger the workspace's `-Werror` warnings.
- Optional dependencies are only fetched when their feature is on (`FLINT_ENABLE_TRACY`, `FLINT_BUILD_TESTS`).
- Pinned versions (November 2026): GoogleTest v1.18.0, Tracy v0.15.0.

## Consequences

- No extra tool is needed; the same CMake works on every platform.
- Each build tree downloads and builds its dependencies once; ccache (in the dev container) makes rebuilds cheap.
- Upgrading a dependency is a one-line tag change, reviewed like code.
- System libraries that SDL3 needs on Linux (X11, Wayland, ALSA headers) still come from the OS; the dev container installs them.
