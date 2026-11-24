#pragma once

#include <cmath>
#include <concepts>
#include <type_traits>
#include <utility>

#include "flint/event/signal.hpp"

namespace flint::model {

/// Reports whether two values are the same for change notification. Like `==`, except that two NaNs are the same, so
/// assigning NaN to a NaN-valued property does not notify again.
template <std::equality_comparable Value>
[[nodiscard]] constexpr auto IsSameValue(const Value& left, const Value& right) -> bool {
  if constexpr (std::is_floating_point_v<Value>) {
    return left == right || (std::isnan(left) && std::isnan(right));
  } else {
    return left == right;
  }
}

/// A value that notifies when it changes: the building block of view models (flint ADR-0009, agate ADR-0003). agate's
/// `{Binding Path}` finds Observable members by name through reflection, so a view model is a plain struct of them.
///
/// Assigning a value equal to the current one does not notify. Observables are neither copyable nor movable, because
/// bindings and connections refer to them.
///
/// Not thread-safe: change it on the thread that observes it (for agate, the UI thread).
///
/// @code
/// struct PersonViewModel {
///   flint::model::Observable<std::string> Name{"Ada"};
///   flint::model::Observable<int> Age{36};
/// };
///
/// person.Name = "Grace";        // notifies
/// std::string name = person.Name;
/// bool empty = person.Name->empty();
/// @endcode
template <std::equality_comparable Value>
class Observable {
public:
  using ValueType = Value;

  /// Starts with a value-initialised value.
  Observable()
    requires std::default_initializable<Value>
  = default;

  /// Starts with a value. Does not notify.
  explicit Observable(Value initial) : m_value{std::move(initial)} {}

  ~Observable() = default;

  Observable(const Observable&) = delete;
  auto operator=(const Observable&) -> Observable& = delete;
  Observable(Observable&&) = delete;
  auto operator=(Observable&&) -> Observable& = delete;

  /// Changes the value, notifying if it differs from the current one.
  auto operator=(Value value) -> Observable& {
    Set(std::move(value));
    return *this;
  }

  /// Changes the value, notifying if it differs from the current one.
  /// @return Whether the value changed.
  auto Set(Value value) -> bool {
    if (IsSameValue(m_value, value)) {
      return false;
    }
    m_value = std::move(value);
    Changed.Emit(m_value);
    return true;
  }

  /// Changes the value in place, then notifies. Use for containers and other values that are expensive to copy. Always
  /// notifies, because the old value is not kept for comparison.
  /// @param mutate Called with a reference to the value.
  template <std::invocable<Value&> Mutator>
  auto Modify(Mutator&& mutate) -> void {
    std::forward<Mutator>(mutate)(m_value);
    Changed.Emit(m_value);
  }

  [[nodiscard]] auto Get() const noexcept -> const Value& { return m_value; }

  // NOLINTNEXTLINE(google-explicit-constructor): an observable reads as its value, like std::atomic
  [[nodiscard]] operator const Value&() const noexcept { return m_value; }

  [[nodiscard]] auto operator*() const noexcept -> const Value& { return m_value; }

  [[nodiscard]] auto operator->() const noexcept -> const Value* { return &m_value; }

  /// Emitted after the value changes, with the new value.
  event::Signal<const Value&> Changed;

private:
  Value m_value{};
};

}  // namespace flint::model
