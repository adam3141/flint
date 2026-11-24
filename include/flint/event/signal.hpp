#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "flint/event/connection.hpp"

namespace flint::event {

/// A list of handlers called in connection order when the signal is emitted (flint ADR-0009). Connect returns a scoped
/// Connection.
///
/// Emitting is re-entrant and tolerates changes made by handlers:
/// - a handler may disconnect itself or any other handler; disconnected handlers are not called again;
/// - handlers connected during an emission are first called by the next emission;
/// - a handler may destroy the signal (for example by destroying its owner); no further handlers are called.
///
/// An unconnected signal is one null shared pointer and allocates nothing.
///
/// Not thread-safe: connect, disconnect and emit on one thread (for agate, the UI thread). To notify another thread,
/// post to its dispatcher.
///
/// @code
/// flint::event::Signal<int, std::string_view> progress;
/// flint::event::Connection connection = progress.Connect([](int percent, std::string_view step) { Show(percent, step); });
/// progress.Emit(40, "compiling");
/// @endcode
template <typename... Args>
class Signal {
public:
  using Handler = std::function<void(Args...)>;

  Signal() = default;

  /// Disconnects every handler, so an emission in progress calls no more of them.
  ~Signal() { DisconnectAll(); }

  Signal(const Signal&) = delete;
  auto operator=(const Signal&) -> Signal& = delete;

  /// Takes over the other signal's handlers.
  Signal(Signal&& other) noexcept : m_state{std::exchange(other.m_state, nullptr)} {}

  /// Disconnects this signal's handlers and takes over the other signal's.
  auto operator=(Signal&& other) noexcept -> Signal& {
    if (this != &other) {
      DisconnectAll();
      m_state = std::exchange(other.m_state, nullptr);
    }
    return *this;
  }

  /// Adds a handler.
  /// @param handler Called with the emitted arguments.
  /// @return The connection. The handler is disconnected when it is destroyed, unless it is released.
  [[nodiscard]] auto Connect(Handler handler) -> Connection {
    if (m_state == nullptr) {
      m_state = std::make_shared<State>();
    }
    if (m_state->emitDepth == 0) {
      Compact(*m_state);
    }
    auto slot = std::make_shared<Slot>(std::move(handler));
    std::weak_ptr<detail::SlotBase> weak = slot;
    m_state->slots.push_back(std::move(slot));
    return Connection{std::move(weak)};
  }

  /// Calls every connected handler, in connection order.
  auto Emit(Args... args) -> void {
    if (m_state == nullptr) {
      return;
    }
    // Keeps the handler list alive if a handler destroys this signal.
    const std::shared_ptr<State> state = m_state;
    ++state->emitDepth;
    const std::size_t count = state->slots.size();
    for (std::size_t index = 0; index < count; ++index) {
      // Slots are only removed when no emission is running, so the reference stays valid while the handler runs.
      const Slot& slot = *state->slots[index];
      if (slot.IsConnected()) {
        slot.Invoke(args...);
      }
    }
    --state->emitDepth;
    if (state->emitDepth == 0) {
      Compact(*state);
    }
  }

  /// Disconnects every handler.
  auto DisconnectAll() noexcept -> void {
    if (m_state == nullptr) {
      return;
    }
    for (const auto& slot : m_state->slots) {
      slot->Disconnect();
    }
  }

  /// Returns the number of connected handlers.
  [[nodiscard]] auto GetHandlerCount() const noexcept -> std::size_t {
    if (m_state == nullptr) {
      return 0;
    }
    return static_cast<std::size_t>(std::ranges::count_if(m_state->slots, [](const auto& slot) { return slot->IsConnected(); }));
  }

  /// Reports whether any handler is connected.
  [[nodiscard]] auto HasHandlers() const noexcept -> bool { return GetHandlerCount() > 0; }

private:
  class Slot final : public detail::SlotBase {
  public:
    explicit Slot(Handler handler) : m_handler{std::move(handler)} {}

    auto Invoke(Args... args) const -> void { m_handler(args...); }

  private:
    Handler m_handler;
  };

  struct State {
    std::vector<std::shared_ptr<Slot>> slots;
    int emitDepth{0};
  };

  static auto Compact(State& state) -> void {
    std::erase_if(state.slots, [](const auto& slot) { return !slot->IsConnected(); });
  }

  std::shared_ptr<State> m_state;
};

}  // namespace flint::event
