#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace flint::platform {

/// Returns the operating-system identifier of the calling thread (the TID on Linux), as shown by debuggers and `top`.
[[nodiscard]] auto CurrentThreadId() noexcept -> std::uint64_t;

/// Names the calling thread. The name is visible to debuggers and profilers, and is recorded in log records.
/// Names longer than the platform limit (15 characters on Linux) are truncated for the OS, but kept in full for flint.
/// @param name The new name of the calling thread.
auto SetCurrentThreadName(std::string_view name) -> void;

/// Returns the name given to the calling thread with SetCurrentThreadName, or an empty string if it has none.
[[nodiscard]] auto CurrentThreadName() -> const std::string&;

}  // namespace flint::platform
