#pragma once

#include <functional>
#include <utility>

#include "flint/log/record.hpp"
#include "flint/log/sink_interface.hpp"

namespace flint::log {

/// Passes each record to a function. Useful for showing logs in an application's own UI, and in tests.
///
/// In asynchronous mode the function runs on the logger's worker thread; it must hand data to other threads safely and
/// must not log through the same logger.
class CallbackSink final : public SinkInterface {
public:
  using Callback = std::function<void(const Record&)>;

  explicit CallbackSink(Callback callback) : m_callback{std::move(callback)} {}

  auto Write(const Record& record) -> void override { m_callback(record); }

  auto Flush() -> void override {}

private:
  Callback m_callback;
};

}  // namespace flint::log
