#pragma once

#include <string>

#include "flint/log/record.hpp"
#include "flint/log/sink_interface.hpp"

namespace flint::log {

/// Sends records to the Tracy profiler's message timeline, coloured by level, so log output lines up with profiling
/// zones. When flint is built without FLINT_ENABLE_TRACY, this sink discards everything.
class TracySink final : public SinkInterface {
public:
  auto Write(const Record& record) -> void override;

  auto Flush() -> void override {}

private:
  std::string m_buffer;
};

}  // namespace flint::log
