# 0004. Logger architecture: severities, categories, sinks, async

- Status: Accepted
- Date: 2026-11-24

## Context

The owner asked for:

- Console (ANSI colour), file, JSON file and network sinks.
- The levels Fatal, Error, Warning, Info, Debug and Trace, plus **custom levels that can be filtered**, ideally declared in a configuration file.
- The source file, line and unmangled function name, the date and time, the machine's uptime, and the application's uptime measured from the first Logger.

They chose "both" for custom levels: severity levels that slot between the standard ones, **and** independent categories (channels). The UI must stay fluid, so logging must be cheap on the calling thread.

## Decision

- **Level** is a value type wrapping a 16-bit severity. Standard levels are 100 (Trace) to 600 (Fatal), spaced 100 apart so custom levels fit between them (for example "Verbose" = 150). Names and colours are kept in a process-wide registry. Configuration files can register levels.
- **Category** is a 16-bit id with a registered name. There are up to 1024 per process. `Channel` binds a category to a logger for convenient use: `kLayoutLog.Debug(...)`.
- **Filter** (per sink) has a default minimum level and per-category overrides. `kOff` disables a category.
- **Logger** routes records to sinks. It keeps a per-category threshold table, which is the lowest level any sink accepts. A disabled call therefore costs one relaxed atomic load and never formats (measured at about 4 ns).
- **Asynchronous by default.** The calling thread formats the message, then pushes the record onto a bounded queue. A worker thread swaps the whole queue out and writes the batch, flushing once per batch. Overflow either blocks (the default) or drops and counts. `Mode::Synchronous` writes on the calling thread, for tests and short tools.
- **Fatal and above are flushed before the call returns.** Logging at Fatal does **not** terminate the process; that is the caller's decision. There is deliberately no option to make it terminate (owner decision, 2026-11-25).
- **Call-site capture without macros.** The format-string parameter type (`FormatString<Args...>`) has a `consteval` constructor with a defaulted `std::source_location` argument. It captures the caller's location and checks the format at compile time.
- **Record** carries the level, category, message, file, line, column and function signature (unmangled by `std::source_location`), timestamp, application uptime, system uptime (`CLOCK_BOOTTIME` on Linux), thread id and name, and sequence number.
- **Sinks** implement `SinkInterface`. The logger calls a given sink from only one thread at a time. Sinks are held in an immutable list, swapped under a mutex, so the worker never calls a sink while holding the logger's locks.
- **JSON output is JSON Lines** (one object per line). It stays valid after a crash, can be appended to, and can be streamed by `jq`.

## Consequences

- Severity ordering and category enablement are independent, which covers both styles of filtering the owner described.
- There is no compile-time removal of log statements, because that would need macros. Disabled statements cost a few nanoseconds; if that ever matters on a hot path, guard with `IsEnabled`.
- The default logger is a function-local static. Logging from static destructors after `main` returns is unsupported.
- Benchmarks are in `examples/bench_demo.cpp`. See the API guide for numbers.
