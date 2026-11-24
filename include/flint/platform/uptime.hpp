#pragma once

#include <chrono>
#include <expected>
#include <system_error>

namespace flint::platform {

/// Returns how long the machine has been running since boot, including time spent suspended where the OS reports it.
/// @return The system uptime, or the OS error that prevented reading it.
[[nodiscard]] auto SystemUptime() -> std::expected<std::chrono::nanoseconds, std::error_code>;

}  // namespace flint::platform
