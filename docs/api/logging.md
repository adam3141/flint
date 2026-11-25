# Logging

```cpp
#include "flint/log/log.hpp"      // default logger, free functions, Channel
#include "flint/log/config.hpp"   // ConfigureFromFile
```

## Quick start

```cpp
namespace log = flint::log;

auto main() -> int {
  log::Info("starting");                              // console, Info and above, by default
  if (auto ok = log::ConfigureFromFile(log::DefaultLogger(), "logging.json"); !ok.has_value()) {
    log::Error("{}", ok.error().Describe());
  }
  log::Debug("window {}x{}", 1280, 720);              // format is checked at compile time
}
```

Every record captures the call site automatically (file, line, column and unmangled function signature), with no macros.

## Levels

| Level | Severity | Default colour |
|---|---|---|
| `kTrace` | 100 | grey |
| `kDebug` | 200 | cyan |
| `kInfo` | 300 | green |
| `kWarning` | 400 | bright yellow |
| `kError` | 500 | bright red |
| `kFatal` | 600 | white on red |
| `kOff` | 65535 | (a threshold that accepts nothing) |

Free functions `log::Trace` to `log::Fatal` write to the default logger. `log::Log(level, ...)` takes any level.

`Fatal` records are written and flushed before the call returns. **Logging at Fatal does not terminate the process.**

### Custom levels

```cpp
const auto verbose = log::RegisterLevel("Verbose", 150, log::Colour::Magenta);   // between Trace and Debug
if (verbose.has_value()) {
  log::Log(*verbose, "very chatty detail: {}", detail);
}
const auto notice = log::FindLevel("notice");   // case-insensitive lookup; levels usually come from config
```

Registering the same name, severity and colour again is a no-op, so configuration can be reloaded. Conflicting registrations return `LevelError::NameInUse` or `LevelError::SeverityInUse`.

## Categories and channels

Categories are filtered independently of severity. A `Channel` is a category bound to a logger:

```cpp
const log::Channel kLayoutLog{"layout"};   // registers the category; safe at namespace scope

kLayoutLog.Debug("measured {} children", count);
kLayoutLog.Log(*verbose, "...");
if (kLayoutLog.IsEnabled(log::kTrace)) { /* expensive diagnostics */ }
```

The default category is `app` (`log::kDefaultCategory`).

## Filters

Each sink has a `Filter`: a default minimum, plus per-category overrides.

```cpp
log::Filter filter{log::kInfo};
filter.SetCategoryMinimum(*log::RegisterCategory("render"), log::kWarning);
filter.SetCategoryMinimum(*log::RegisterCategory("binding"), log::kOff);   // disable
logger.AddSink(std::move(sink), filter);
```

A record is formatted only if at least one sink accepts it. A disabled call costs about 4 ns.

## Loggers

`log::DefaultLogger()` is the process-wide logger used by the free functions and by `Channel{name}`. Create your own when you need separate routing:

```cpp
log::Logger logger{{.mode = log::Mode::Asynchronous,     // or Synchronous
                    .queueCapacity = 8192,
                    .overflow = log::OverflowPolicy::Block,   // or Drop (counted by DroppedCount())
                    .threadName = "flint.log"}};
const log::SinkId id = logger.AddSink(std::move(sink), log::Filter{log::kDebug});
logger.Info("hello {}", "world");
logger.Flush();            // waits until everything submitted so far is written and flushed
logger.RemoveSink(id);     // flushes first
```

Application uptime is measured from `Logger::ApplicationEpoch()`, which is when the first `Logger` in the process was constructed.

## Sinks

| Sink | Create | Notes |
|---|---|---|
| `ConsoleSink` | `ConsoleSink::Create({.stream, .colour, .pattern, .timeZone})` | ANSI colour when `colour` is `Auto` and the stream is a terminal; honours `NO_COLOR` and `FORCE_COLOR`; Windows VT mode enabled automatically |
| `FileSink` | `FileSink::Create({.path, .pattern, .timeZone, .rotation})` | Creates directories; `rotation = {.maxBytes, .maxFiles}` keeps `app.1.log` (newest) to `app.N.log` |
| `JsonFileSink` | `JsonFileSink::Create({.path, .rotation})` | JSON Lines, one object per record |
| `NetworkSink` | `std::make_unique<NetworkSink>(NetworkSinkOptions{.host, .port})` | TCP JSON Lines on its own thread; buffers while disconnected, reconnects with back-off |
| `TracySink` | `std::make_unique<TracySink>()` | Tracy message timeline; does nothing without `FLINT_ENABLE_TRACY` |
| `CallbackSink` | `std::make_unique<CallbackSink>(fn)` | Calls `fn(const Record&)` on the logger's worker thread, e.g. for an in-app log pane |

Write your own by implementing `SinkInterface` (`Write(const Record&)` and `Flush()`). A logger calls each sink from one thread at a time. A sink must not log through its own logger.

## Patterns

Text sinks use `PatternFormatter` patterns. Fields are written `{name}` or `{name:spec}`, where spec is a `std::format` spec applied to the field's text:

| Field | Example |
|---|---|
| `timestamp` | `2026-11-24 20:15:03.123456` |
| `iso8601` | `2026-11-24T09:15:03.123456Z` (always UTC) |
| `date` / `time` | `2026-11-24` / `20:15:03.123` |
| `level` / `severity` | `Warning` / `400` |
| `category` | `layout` |
| `message` | the text |
| `file` / `filename` / `line` | `/src/panel.cpp` / `panel.cpp` / `42` |
| `function` / `func` | `void agate::Panel::Measure(agate::Size)` / `agate::Panel::Measure` |
| `thread` / `threadname` | `12345` / `main` (set with `flint::platform::SetCurrentThreadName`) |
| `uptime` / `sysuptime` | application / machine uptime in seconds |
| `sequence` | position in the logger's order |

The defaults are `PatternFormatter::kConsolePattern` and `PatternFormatter::kFilePattern`.

## JSON record format

Written by `JsonFileSink` and `NetworkSink` (see `AppendJsonRecord`):

```json
{"timestamp":"2026-11-24T09:57:32.626523Z","level":"Info","severity":300,"category":"app",
 "message":"...","file":"/workspaces/silica/flint/examples/log_demo.cpp","line":59,"column":13,
 "function":"int {anonymous}::Run(std::span<char*>)","thread":5681,"threadName":"main",
 "appUptimeNs":15345085,"systemUptimeNs":204344554556366,"sequence":2,"pid":5681,"host":"devbox"}
```

`RecordFromJson` rebuilds a record from this format, which `flint-logcat` uses.

## Configuration file

`ConfigureFromFile(logger, path)` or `ConfigureFromText(logger, text)`. The file is read with glaze (ADR-0012) into `log::LoggingConfig`. Comments are allowed; trailing commas and unknown keys are not. Enum values are case-insensitive.

```jsonc
{
  "levels":     [ { "name": "Verbose", "severity": 150, "colour": "magenta" } ],
  "categories": [ "layout", "render", "binding" ],
  "sinks": [
    { "type": "console",
      "minLevel": "Verbose",                 // a level name or a number
      "stream": "stderr",                    // stdout | stderr
      "colour": "auto",                      // auto | always | never
      "pattern": "{time} {level:<7} {message}",
      "timeZone": "local",                   // local | utc
      "categories": { "render": "Warning", "binding": "Off" } },
    { "type": "file",    "path": "logs/app.log",   "maxBytes": 10485760, "maxFiles": 5 },
    { "type": "json",    "path": "logs/app.jsonl", "maxBytes": 10485760, "maxFiles": 5 },
    { "type": "network", "host": "127.0.0.1", "port": 9020,
      "bufferCapacity": 10000, "reconnectDelayMs": 250, "maxReconnectDelayMs": 5000, "connectTimeoutMs": 1000 },
    { "type": "tracy" }
  ]
}
```

Colours: `default`, `black`, `red`, `green`, `yellow`, `blue`, `magenta`, `cyan`, `white`, `grey`, the `bright*` variants (for example `brightRed`), and `alarm`.

JSON that cannot be read gives a line and column, for example ``malformed JSON at line 3, column 4: unknown key in `"sinkz": []` ``. Values that are read but make no sense give a JSON Pointer, for example `unknown level at /sinks/0/minLevel: no level named 'Loud'`. On error the logger keeps its previous sinks.

`LoggingConfig` is a plain struct, so it can be part of an application's own configuration, read with glaze in one go, and passed to `Configure(logger, config)`. It can also be built in code:

```cpp
log::LoggingConfig config;
config.sinks.push_back({.type = "console", .minLevel = log::LevelSetting{"Warning"}});
config.sinks.push_back({.type = "json", .path = "logs/app.jsonl"});
if (auto ok = log::Configure(log::DefaultLogger(), config); !ok.has_value()) { /* ok.error().Describe() */ }
```

## flint-logcat

```bash
flint-logcat                     # listen on 127.0.0.1:9020 and pretty-print
flint-logcat --port 9100 --bind 0.0.0.0
flint-logcat --pattern "{time} {level} {message}"
flint-logcat --raw | jq .        # pass JSON through
```
