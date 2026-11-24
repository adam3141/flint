# 0003. Reflection confined to `flint/meta`, with tooling stubs

- Status: Accepted
- Date: 2026-11-24

## Context

GCC 16 compiles C++26 reflection, but clangd and clang-tidy (LLVM 23.1) do not implement it. Any file that contains reflection syntax shows errors in the editor and cannot be linted. The owner chose to confine reflection to a few headers so that clang tooling stays clean everywhere else.

A first attempt declared the meta function templates without bodies when reflection was unavailable. clang rejected that: a `constexpr` (implicitly inline) template that is used must be defined, and templates instantiated with types in anonymous namespaces cannot be defined elsewhere.

## Decision

1. `flint/meta/reflection.hpp` is the only place that tests for reflection. It defines `FLINT_HAS_REFLECTION`:
   - `1` when `__cpp_impl_reflection >= 202506L` (GCC 16 with `-freflection`).
   - `0` when `FLINT_REFLECTION_TOOLING_STUBS` is defined. Only clang tooling defines it: `.clangd` adds it, and `tools/tidy.sh` passes it to clang-tidy.
   - Otherwise `#error`. A real build without reflection fails, so stubs can never reach a binary.
2. Public meta headers (such as `flint/meta/enum.hpp`) declare their templates unconditionally. They then include either `detail/*_impl.hpp` (the reflection bodies) or stub bodies that return empty values.
3. Reflection syntax (`^^`, `[: :]`, `template for`, `<meta>`) appears **only** in `flint/meta/detail/*_impl.hpp`, and later in agate's equivalent `meta` directory.
4. Code outside the meta headers calls meta functions only at run time or in ordinary `constexpr` code, never in a `static_assert` or another constant expression. Under the stubs that would fail in clang tooling.

## Consequences

- clangd and clang-tidy work on every file except the `*_impl.hpp` files, which are expected to show errors in the editor until LLVM supports P2996.
- Each new reflection facility costs a declaration, an implementation and a stub. This is deliberate friction: reflection stays in a small, well-tested surface.
- When clang gains reflection, step 1's first branch takes over automatically for it and the stubs can be deleted.
