# 0012. JSON is read and written with glaze

- Status: Accepted (supersedes ADR-0005; amends ADR-0006)
- Date: 2026-11-25

## Context

ADR-0005 gave flint its own JSON value, parser and writer. The owner asked why, given that glaze exists, and decided (2026-11-25) that flint uses established libraries for commodity parsing. The owner chose to use glaze directly rather than keep `flint::json` as a wrapper.

glaze (MIT) reads and writes C++ structs by reflection, with no per-type code. v9.1.0 uses C++26 reflection (P2996) when GCC 16 is given `-freflection`, builds cleanly under the workspace warnings as a system include, and parses under clang 23 for clangd and clang-tidy, where it falls back to its older reflection technique.

## Decision

- `flint::json` (Value, Parse, Writer) is removed. glaze v9.1.0 is a public, header-only dependency of flint (FetchContent).
- The logging configuration is a plain struct, `log::LoggingConfig`, that glaze reads. Applications can embed it in their own configuration structs. `ConfigureFromText` reads it with comments allowed and unknown keys as errors.
- Read errors (bad syntax, wrong types, misspelt keys) report a line and column, glaze's reason and the offending line. Checks that need meaning (unknown level names, sink types, enum values, missing fields) still report a JSON Pointer such as `/sinks/1/minLevel`.
- JSON Lines records are written and read through a reflected struct. Reading ignores unknown fields, so newer writers can add fields.
- Types that glaze reflects must not be in an anonymous namespace: clang tooling's fallback reflection needs them to have linkage. Use a named `detail` namespace.
- The planned reflection-based JSON (de)serialisation in `flint::meta` is dropped; glaze covers it.

## Consequences

- About 1,000 fewer lines to maintain. Writing a JSON Lines record is 2.5 times faster (186 ns against 462 ns), and reading one back takes less time than the old parser needed just to build its tree (749 ns against 1.17 µs), in release builds.
- Trailing commas are no longer accepted in configuration files: glaze does not support them. Comments still are.
- Code that included `flint/json/*.hpp` must use glaze instead. Nothing in agate or opal did.
- glaze headers are heavy; only the files that read or write JSON include them.
