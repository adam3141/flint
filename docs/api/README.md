# flint API guide

How to use flint, by module. Every public declaration is also documented in its header with `///` comments; these guides show how the pieces fit together.

| Guide | Headers |
|---|---|
| [Logging](logging.md) | `flint/log/*.hpp` |
| [JSON](json.md) | `flint/json/*.hpp` |
| [Benchmarking and profiling](bench-and-profile.md) | `flint/bench/*.hpp`, `flint/profile/profile.hpp` |
| [Platform, networking and text](platform-net-text.md) | `flint/platform/*.hpp`, `flint/net/*.hpp`, `flint/text/*.hpp` |
| [Reflection utilities](meta.md) | `flint/meta/*.hpp` |
| [Signals, observables and commands](events-and-models.md) | `flint/event/*.hpp`, `flint/model/*.hpp` |

## Using flint in a CMake project

Inside the silica workspace, flint is already part of the build. Standalone:

```cmake
add_subdirectory(flint)          # or FetchContent_Declare(flint GIT_REPOSITORY https://github.com/adam3141/flint.git ...)
target_link_libraries(my_app PRIVATE flint::flint)
flint_target_options(my_app)     # optional: the workspace warning set, -Werror and sanitizers
```

You need GCC 16 or newer; flint adds `-freflection` to its users automatically.

## Error handling convention

Fallible functions return `std::expected<T, E>`. Each error type has a `Describe()` method (or a `Describe(error)` free function) that returns a readable message:

```cpp
if (auto result = flint::log::ConfigureFromFile(logger, "logging.json"); !result.has_value()) {
  flint::log::Error("bad logging config: {}", result.error().Describe());
}
```
