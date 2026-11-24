# Signals, observables and commands

```cpp
#include "flint/event/signal.hpp"       // Signal<Args...>
#include "flint/event/connection.hpp"   // Connection, ConnectionGroup
#include "flint/model/observable.hpp"   // Observable<T>, IsSameValue
#include "flint/model/command.hpp"      // Command
```

The building blocks for change notification and view models (ADR-0009). agate's routed events and dependency properties are built on `Signal`. View models are plain structs of `Observable` and `Command` members.

None of these types are thread-safe. Use each one on a single thread, which for agate is the UI thread, and post work across threads through a dispatcher.

## Signals and connections

```cpp
flint::event::Signal<int, std::string_view> progress;

{
  flint::event::Connection connection = progress.Connect([](int percent, std::string_view step) { Show(percent, step); });
  progress.Emit(40, "compiling");   // calls the handler
}                                   // connection destroyed: handler disconnected
progress.Emit(80, "linking");       // calls nothing

progress.Connect(Log).Release();    // stays connected for the signal's lifetime
```

- `Connect` returns a `Connection`, marked `[[nodiscard]]` so that a handler is not disconnected by accident. Destroying or reassigning a `Connection` disconnects its handler. It is move-only and safe to use after the signal is gone.
- `ConnectionGroup` holds several connections. Add with `group += signal.Connect(...)`, and use `DisconnectAll()` to drop them together.
- Handlers run in connection order. During an emission a handler may:
  - disconnect any handler, including itself;
  - connect new handlers, which first run on the next emission;
  - emit again (recursively);
  - destroy the signal, after which no further handlers run.
- `Signal<const T&>` passes references through without copying.

## Observable values

```cpp
struct PersonViewModel {
  flint::model::Observable<std::string> Name{"Ada"};
  flint::model::Observable<int> Age{36};
  flint::model::Observable<std::vector<std::string>> Tags;
};

PersonViewModel person;
auto connection = person.Name.Changed.Connect([](const std::string& name) { std::println("now {}", name); });
person.Name = "Grace";                     // prints "now Grace"
person.Name = "Grace";                     // equal: no notification
std::string name = person.Name;            // reads as its value
bool empty = person.Name->empty();
person.Tags.Modify([](auto& tags) { tags.push_back("pioneer"); });   // in place; always notifies
```

- `Set(value)` returns whether the value changed. NaN counts as equal to NaN (`IsSameValue`), so a NaN that is assigned again does not notify.
- Observables are neither copyable nor movable, because bindings and connections refer to them.
- Members are named in PascalCase, because agate's `{Binding Path}` finds them by name through reflection (coding-style.md section 7).

## Commands

```cpp
struct EditorViewModel {
  flint::model::Observable<std::string> Text;
  flint::model::Command Save{[this] { Write(Text); }, [this] { return !Text->empty(); }};
};

EditorViewModel editor;
editor.Text.Changed.Connect([&](const std::string&) { editor.Save.NotifyCanExecuteChanged(); }).Release();
editor.Save.Execute();   // runs only if CanExecute(); returns whether it ran
```

agate binds buttons and menu items to commands and disables them while `CanExecute()` is false. It re-asks whenever `CanExecuteChanged` is emitted.
