# 0013. View models are reflected by flint, and lists are ObservableVector

- Status: Accepted
- Date: 2026-11-25

## Context

agate milestone 4 adds data binding. The owner chose WPF-style `{Binding Path}` resolved at run time by name, view models made of public `Observable<T>` and `Command` members found by reflection (no base class, no string notifications), and basic lists: an observable collection that an ItemsControl shows through a DataTemplate. The view refers to a view model the application owns.

Finding members by name and touching them without knowing their types is not UI code, so it belongs in flint, next to `Observable` and `Command` (ADR-0009).

## Decision

- `flint::model::ModelClassOf<T>()` reflects over T's **public** non-static data members once and describes each bindable one as a `MemberInfo`:
  - `Observable<V>`: read (as `std::any`), written (from a `std::any` holding exactly V) and watched;
  - `Command`;
  - `ObservableVector<V>`: size, items as `ObjectRef`, and change notifications;
  - a nested model (any class with bindable members, recursively), so paths such as `Address.City` work;
  - any other copyable member, as a read-only Value.
- `ObjectRef` pairs an object pointer with its `ModelClass`. It does not own the object.
- `ObservableVector<T>` owns its items through `unique_ptr`, so items never move and can be view models made of Observables. It emits `Inserted(index)` after adding, `Removing(index)` *before* destroying (views let go first), and `Resetting`/`Reset` around `Clear`.
- Reflection is confined to `flint/model/meta/detail/model_class_impl.hpp` (ADR-0003). Types are named with `display_string_of`.

## Consequences

- View models need no registration, macros or base class. Private members stay private to views.
- Type conversion between a member's type and a property's type is agate's job (converters, ADR in agate).
- Pointers to view models and `Observable<T*>` are not followed yet; nested models are members by value.
- The struct reflection task ("ForEachMember") is covered by ModelClass for now.
