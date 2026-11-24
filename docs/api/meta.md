# Reflection utilities (`flint/meta`)

C++26 static reflection helpers. They need GCC 16 with `-freflection`, which flint adds for you. See ADR-0003 for how clang tooling is kept working.

## Enums

```cpp
#include "flint/meta/enum.hpp"

enum class Mineral : std::uint8_t { Topaz, Ruby, Malachite };

flint::meta::EnumName(Mineral::Ruby);                        // "Ruby" (static storage)
flint::meta::EnumFromName<Mineral>("Malachite");             // optional<Mineral>
flint::meta::EnumFromNameIgnoreCase<Mineral>("malachite");   // for configuration files
flint::meta::EnumValues<Mineral>();                          // span: Topaz, Ruby, Malachite
flint::meta::EnumCount<Mineral>();                           // 3
```

An unknown value gives an empty name. Use these functions at run time or in ordinary `constexpr` code, but not in `static_assert`: clang tooling sees stub bodies (ADR-0003).

## Planned

- Struct member reflection (`ForEachMember`), for JSON (de)serialisation and agate view models.
- Annotation-driven property metadata for agate dependency properties.
