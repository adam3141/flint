# 0005. flint's own JSON library

- Status: Accepted
- Date: 2026-11-24

## Context

JSON is needed for configuration files, the JSON log sinks, the network sink, flint-logcat, and later agate themes and settings. The common choice, nlohmann/json, throws by default, favours convenience over speed, and its non-throwing mode is awkward with the guide's `std::expected` error handling.

## Decision

flint provides `flint::json`:

- `Value` (null, bool, int64, double, string, `Array`, `Object`) has an **ordered** `Object`, so files written back out keep their order.
- `Parse` returns `std::expected<Value, ParseError>`, with line and column. It allows comments and trailing commas only when asked (`kConfigParseOptions`), and limits nesting depth.
- `Writer` streams JSON straight into a `std::string` with no tree, for hot paths such as log sinks. `Serialize` writes a `Value`.

## Consequences

- No throwing adapter is needed, and errors fit the guide.
- About 700 lines are maintained here. They are covered by unit tests, including escapes, surrogate pairs, number edge cases and nesting limits.
- Reflection-based (de)serialisation of structs (`json::ToJson(object)`) is a natural follow-up in `flint/meta`.
