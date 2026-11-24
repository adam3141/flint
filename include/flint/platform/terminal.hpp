#pragma once

#include <cstdint>

namespace flint::platform {

/// A standard output stream of the process.
enum class StandardStream : std::uint8_t {
  Output,
  Error,
};

/// Reports whether a standard stream is attached to an interactive terminal.
/// @param stream The stream to test.
[[nodiscard]] auto IsTerminal(StandardStream stream) noexcept -> bool;

/// Reports whether ANSI colour output should be used for a stream, following the NO_COLOR and FORCE_COLOR conventions
/// and falling back to IsTerminal. On Windows this also switches the console into virtual-terminal mode.
/// @param stream The stream that will receive coloured output.
[[nodiscard]] auto SupportsAnsiColour(StandardStream stream) noexcept -> bool;

}  // namespace flint::platform
