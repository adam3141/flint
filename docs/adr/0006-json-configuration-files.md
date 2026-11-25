# 0006. JSON (with comments) for configuration files

- Status: Accepted, amended by [ADR-0012](0012-json-with-glaze.md): files are read with glaze, which allows comments but not trailing commas
- Date: 2026-11-24

## Context

The logger needs a configuration file that can declare custom levels and categories. Other flint and agate settings will need files too. The candidates were JSON, TOML and XML. The owner chose JSON.

## Decision

- Configuration files are JSON, parsed with `json::kConfigParseOptions`, which allows `//` and `/* */` comments and trailing commas.
- Data files exchanged with other programs (JSON Lines logs) are strict JSON.
- Configuration errors report a JSON Pointer to the offending value (`/sinks/1/minLevel`) and a reason.
- Enum-valued settings (`"colour": "brightMagenta"`, `"timeZone": "utc"`) are matched to C++ enumerators by reflection, ignoring case. Renaming an enumerator therefore renames the setting.

## Consequences

- One parser and writer covers configuration and log data.
- Applying a configuration builds every sink before it changes the logger, so a bad file leaves the previous configuration in place.
