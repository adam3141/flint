# 0009. Signals, observables and commands live in flint

- Status: Accepted
- Date: 2026-11-25

## Context

agate's routed events, property change notification and MVVM need a signal type with scoped connections. View models should be testable without agate. The owner chose `Observable<T>` view-model members in flint, and connections that disconnect when destroyed (2026-11-25).

## Decision

- `flint::event::Signal<Args...>` calls handlers in connection order. `Connect` returns a move-only `flint::event::Connection` that disconnects when destroyed; `Release()` keeps the handler for the signal's lifetime. `ConnectionGroup` holds several connections together.
- Emission is re-entrant and survives changes made by handlers. A handler may:
  - disconnect any handler, including itself;
  - connect new handlers, which first run on the next emission;
  - destroy the signal, after which no further handlers run.
- An unconnected signal allocates nothing.
- `flint::model::Observable<T>` is a value that emits `Changed` when it changes. Assigning an equal value does not notify, and two NaNs count as equal. `Modify` changes a value in place and always notifies.
- `flint::model::Command` wraps an action and an optional `CanExecute` condition, with a `CanExecuteChanged` signal.
- None of these are thread-safe. Use them on one thread, and cross threads through a dispatcher.

## Consequences

- View models are plain structs of `Observable` and `Command` members, testable in command-line tests. agate's `{Binding}` (milestone 4) will find these members by name through reflection.
- Observables and commands are public data members named in PascalCase. The coding guide allows this for self-guarding members such as these.
- Each connected handler costs one shared allocation, which is what lets a `Connection` safely outlive its signal.
