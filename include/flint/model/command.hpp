#pragma once

#include <functional>

#include "flint/event/signal.hpp"

namespace flint::model {

/// An action a view can invoke, with an optional condition that enables it (flint ADR-0009, agate ADR-0003). agate
/// binds buttons and menu items to Command members of a view model (`Command="{Binding Save}"`) and disables them while
/// CanExecute is false.
///
/// Not thread-safe: use it on the thread that observes it (for agate, the UI thread).
///
/// @code
/// struct PersonViewModel {
///   flint::model::Observable<std::string> Name{"Ada"};
///   flint::model::Command Save{[this] { Store(Name); }, [this] { return !Name->empty(); }};
/// };
///
/// // When something CanExecute depends on changes:
/// Name.Changed.Connect([this](const std::string&) { Save.NotifyCanExecuteChanged(); }).Release();
/// @endcode
class Command {
public:
  using ExecuteFunction = std::function<void()>;
  using CanExecuteFunction = std::function<bool()>;

  /// @param execute Runs the action.
  /// @param canExecute Reports whether the action is available. Empty means always.
  explicit Command(ExecuteFunction execute, CanExecuteFunction canExecute = {});

  ~Command() = default;

  Command(const Command&) = delete;
  auto operator=(const Command&) -> Command& = delete;
  Command(Command&&) = delete;
  auto operator=(Command&&) -> Command& = delete;

  /// Runs the action if it can execute.
  /// @return Whether it ran.
  auto Execute() -> bool;

  /// Reports whether the action is available now.
  [[nodiscard]] auto CanExecute() const -> bool;

  /// Tells views to ask CanExecute again. Call it when anything CanExecute depends on changes.
  auto NotifyCanExecuteChanged() -> void;

  /// Emitted by NotifyCanExecuteChanged.
  event::Signal<> CanExecuteChanged;

private:
  ExecuteFunction m_execute;
  CanExecuteFunction m_canExecute;
};

}  // namespace flint::model
