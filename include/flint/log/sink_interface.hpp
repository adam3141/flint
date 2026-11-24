#pragma once

#include "flint/log/record.hpp"

namespace flint::log {

/// A destination for log records.
///
/// A Logger calls a sink from one thread at a time: its worker thread in asynchronous mode, or the logging thread under
/// the logger's lock in synchronous mode. Implementations therefore need no locking of their own for calls from a
/// single logger. A sink must not log through the logger that owns it.
class SinkInterface {
public:
  virtual ~SinkInterface() = default;

  /// Writes one record. The record has already passed the sink's filter.
  /// @param record The record to write.
  virtual auto Write(const Record& record) -> void = 0;

  /// Makes everything written so far durable or visible, for example by flushing an OS buffer.
  virtual auto Flush() -> void = 0;
};

}  // namespace flint::log
