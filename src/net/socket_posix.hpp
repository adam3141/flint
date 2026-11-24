#pragma once

// Private socket primitives used by TcpClient and TcpListener. The POSIX implementation is in socket_posix.cpp; on
// Windows these return NetError::Code::NotSupported until a Winsock implementation is written (flint TASKS.md).

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "flint/net/tcp_client.hpp"

namespace flint::net::detail {

using Handle = TcpClient::NativeHandle;

auto CloseSocket(Handle handle) noexcept -> void;

[[nodiscard]] auto ConnectSocket(std::string_view host, std::uint16_t port, std::chrono::milliseconds timeout)
    -> std::expected<Handle, NetError>;

[[nodiscard]] auto SendAll(Handle handle, std::span<const std::byte> data) -> std::expected<void, NetError>;

[[nodiscard]] auto Receive(Handle handle, std::span<std::byte> buffer, std::chrono::milliseconds timeout)
    -> std::expected<std::size_t, NetError>;

/// Returns the listening socket and the port it is bound to.
[[nodiscard]] auto ListenSocket(std::string_view address, std::uint16_t port) -> std::expected<std::pair<Handle, std::uint16_t>, NetError>;

[[nodiscard]] auto AcceptSocket(Handle listener, std::chrono::milliseconds timeout) -> std::expected<std::optional<Handle>, NetError>;

}  // namespace flint::net::detail
