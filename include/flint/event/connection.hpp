#pragma once

#include <memory>
#include <utility>
#include <vector>

namespace flint::event {
namespace detail {

/// The part of a connected handler that a Connection can see. Owned by its Signal.
class SlotBase {
public:
  SlotBase() = default;
  virtual ~SlotBase() = default;

  SlotBase(const SlotBase&) = delete;
  auto operator=(const SlotBase&) -> SlotBase& = delete;
  SlotBase(SlotBase&&) = delete;
  auto operator=(SlotBase&&) -> SlotBase& = delete;

  [[nodiscard]] auto IsConnected() const noexcept -> bool { return m_isConnected; }

  auto Disconnect() noexcept -> void { m_isConnected = false; }

private:
  bool m_isConnected{true};
};

}  // namespace detail

/// Ties a handler's lifetime to a scope (flint ADR-0009). Destroying or reassigning a Connection disconnects its
/// handler; Release() lets the handler live as long as the signal. Move-only.
///
/// Not thread-safe: connect, disconnect and emit on one thread (for agate, the UI thread).
///
/// @code
/// flint::event::Connection connection = signal.Connect([](int value) { Use(value); });
/// signal.Connect(OnChanged).Release();   // connected until the signal is destroyed
/// @endcode
class [[nodiscard]] Connection {
public:
  /// An empty connection, connected to nothing.
  Connection() = default;

  /// Used by Signal::Connect.
  explicit Connection(std::weak_ptr<detail::SlotBase> slot) noexcept : m_slot{std::move(slot)} {}

  ~Connection() { Disconnect(); }

  Connection(const Connection&) = delete;
  auto operator=(const Connection&) -> Connection& = delete;

  Connection(Connection&& other) noexcept : m_slot{std::exchange(other.m_slot, {})} {}

  auto operator=(Connection&& other) noexcept -> Connection& {
    if (this != &other) {
      Disconnect();
      m_slot = std::exchange(other.m_slot, {});
    }
    return *this;
  }

  /// Disconnects the handler. The signal never calls it again, even if it is emitting now. Safe to call repeatedly,
  /// and after the signal is destroyed.
  auto Disconnect() noexcept -> void {
    if (const auto slot = m_slot.lock(); slot != nullptr) {
      slot->Disconnect();
    }
    m_slot.reset();
  }

  /// Forgets the handler without disconnecting it, so it stays connected for the signal's lifetime.
  auto Release() noexcept -> void { m_slot.reset(); }

  /// Reports whether the handler is still connected to a living signal.
  [[nodiscard]] auto IsConnected() const noexcept -> bool {
    const auto slot = m_slot.lock();
    return slot != nullptr && slot->IsConnected();
  }

private:
  std::weak_ptr<detail::SlotBase> m_slot;
};

/// Holds several connections and disconnects them together, for example every subscription a view makes.
///
/// @code
/// m_connections += model.Name.Changed.Connect(...);
/// m_connections += model.Age.Changed.Connect(...);
/// @endcode
class ConnectionGroup {
public:
  /// Takes ownership of a connection.
  auto Add(Connection connection) -> void { m_connections.push_back(std::move(connection)); }

  /// Takes ownership of a connection.
  auto operator+=(Connection connection) -> ConnectionGroup& {
    Add(std::move(connection));
    return *this;
  }

  /// Disconnects every connection and empties the group.
  auto DisconnectAll() noexcept -> void { m_connections.clear(); }

  [[nodiscard]] auto GetCount() const noexcept -> std::size_t { return m_connections.size(); }

private:
  std::vector<Connection> m_connections;
};

}  // namespace flint::event
