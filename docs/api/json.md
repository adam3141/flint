# JSON

```cpp
#include "flint/json/value.hpp"
#include "flint/json/parser.hpp"
#include "flint/json/writer.hpp"
```

## Values

```cpp
using namespace flint;
const json::Value theme = json::Object{
    {"name", "Topaz"},
    {"dark", true},
    {"accent", json::Array{255, 200, 0}},
};
theme.Find("name")->AsString();        // std::optional<std::string_view> -> "Topaz"
theme.Find("missing");                 // nullptr
theme.GetKind();                       // json::Kind::Object
```

| Accessor | Returns |
|---|---|
| `AsBool()`, `AsInteger()`, `AsDouble()`, `AsString()` | `std::optional<...>`. `AsInteger` accepts integral doubles; `AsDouble` accepts integers |
| `AsArray()`, `AsObject()` | pointer, or `nullptr` if the kind differs |
| `Find(key)` | member pointer, or `nullptr` |

`Object` keeps insertion order. `Set(key, value)` replaces or appends, `Erase(key)` removes, and `Members()` returns a span of `{key, value}`.

## Parsing

```cpp
auto parsed = json::Parse(text);                                  // strict RFC 8259
auto config = json::Parse(text, json::kConfigParseOptions);       // allows // and /* */ comments and trailing commas
if (!parsed.has_value()) {
  std::println("{}", parsed.error().Describe());                  // "unexpected character at line 3, column 8"
}
```

Integers that fit in 64 bits stay exact; larger numbers become doubles. Nesting deeper than 256 levels is rejected.

## Writing

```cpp
json::Serialize(value);                    // compact
json::Serialize(value, {.indent = 2});     // pretty

std::string out;                           // streaming, no tree: for hot paths
json::Writer writer{out};
writer.BeginObject();
writer.Key("level");
writer.String("Info");
writer.EndObject();
```

NaN and infinity are written as `null`.
