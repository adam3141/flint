# JSON

flint uses [glaze](https://github.com/stephenberry/glaze) for JSON (ADR-0012). It is a public dependency, so linking `flint::flint` makes `<glaze/glaze.hpp>` available. glaze reads and writes structs by reflection, with no per-type code.

```cpp
#include <glaze/glaze.hpp>

namespace app::detail {   // not an anonymous namespace: see below

struct WindowSettings {
  int width{1280};
  int height{800};
  std::optional<std::string> theme;
};

}  // namespace app::detail

app::detail::WindowSettings settings;
constexpr glz::opts kOptions{.null_terminated = false, .comments = true};
if (const glz::error_ctx error = glz::read<kOptions>(settings, text); error) {
  std::println("{}", glz::format_error(error, text));   // "3:4: unknown_key ..." with the offending line
}
std::string out;
(void)glz::write_json(settings, out);                    // {"width":1280,"height":800}
```

## Conventions in flint

- **Options.** Configuration files are read with `.comments = true` (`//` and `/* */`). Unknown keys are errors by default, which catches misspelt keys; set `.error_on_unknown_keys = false` for formats that other versions may extend, as `log::RecordFromJson` does. Pass `.null_terminated = false` when reading from a `std::string_view`.
- **Errors.** `glz::error_ctx::count` is the byte offset of the problem. flint's configuration errors turn it into a line and column (`log::ConfigError`).
- **Optional fields.** Use `std::optional` for fields that may be absent. A struct that callers fill with designated initialisers should give every member an initialiser (`{}`): GCC's `-Wmissing-field-initializers` warns about omitted members without one. Suppress clang-tidy's `readability-redundant-member-init` for such structs, with the reason.
- **No anonymous namespaces.** glaze uses C++26 reflection under GCC 16, but clang tooling (clangd, clang-tidy) falls back to an older technique that needs the type to have linkage. Put reflected types in a named `detail` namespace, with `// NOLINT(misc-use-internal-linkage)` and the reason.
- **Variants.** `std::variant<std::string, std::uint16_t>` reads either a string or a number; see `log::LevelSetting`.

## In flint

| Use | Where |
|---|---|
| Logging configuration | `log::LoggingConfig`, `log::ConfigureFromText` (docs/api/logging.md) |
| JSON Lines records | `log::AppendJsonRecord`, `log::RecordFromJson` |
