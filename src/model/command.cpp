#include "flint/model/command.hpp"

#include <utility>

namespace flint::model {

Command::Command(ExecuteFunction execute, CanExecuteFunction canExecute)
    : m_execute{std::move(execute)}, m_canExecute{std::move(canExecute)} {}

auto Command::Execute() -> bool {
  if (!CanExecute() || !m_execute) {
    return false;
  }
  m_execute();
  return true;
}

auto Command::CanExecute() const -> bool {
  return !m_canExecute || m_canExecute();
}

auto Command::NotifyCanExecuteChanged() -> void {
  CanExecuteChanged.Emit();
}

}  // namespace flint::model
