#pragma once

#include <cstdint>
#include <string>

namespace flint::platform {

/// Returns the identifier of the current process.
[[nodiscard]] auto CurrentProcessId() noexcept -> std::uint64_t;

/// Returns the network host name of this machine, or an empty string if it cannot be determined.
[[nodiscard]] auto HostName() -> std::string;

}  // namespace flint::platform
