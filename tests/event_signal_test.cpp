#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "flint/event/connection.hpp"
#include "flint/event/signal.hpp"

namespace flint::event {
namespace {

TEST(SignalTest, CallsHandlersInConnectionOrder) {
  Signal<int> signal;
  std::vector<std::string> calls;
  const Connection first = signal.Connect([&](int value) { calls.push_back("first " + std::to_string(value)); });
  const Connection second = signal.Connect([&](int value) { calls.push_back("second " + std::to_string(value)); });

  signal.Emit(7);

  EXPECT_EQ(calls, (std::vector<std::string>{"first 7", "second 7"}));
}

TEST(SignalTest, EmittingWithoutHandlersDoesNothing) {
  Signal<> signal;
  signal.Emit();
  EXPECT_FALSE(signal.HasHandlers());
}

TEST(SignalTest, DestroyingConnectionDisconnects) {
  Signal<> signal;
  int calls = 0;
  {
    const Connection connection = signal.Connect([&] { ++calls; });
    signal.Emit();
  }
  signal.Emit();

  EXPECT_EQ(calls, 1);
  EXPECT_EQ(signal.GetHandlerCount(), 0U);
}

TEST(SignalTest, ReleasedConnectionStaysConnected) {
  Signal<> signal;
  int calls = 0;
  signal.Connect([&] { ++calls; }).Release();

  signal.Emit();
  signal.Emit();

  EXPECT_EQ(calls, 2);
}

TEST(SignalTest, MovedConnectionKeepsHandler) {
  Signal<> signal;
  int calls = 0;
  Connection outer;
  {
    Connection inner = signal.Connect([&] { ++calls; });
    outer = std::move(inner);
  }
  signal.Emit();

  EXPECT_EQ(calls, 1);
  EXPECT_TRUE(outer.IsConnected());
}

TEST(SignalTest, ReassigningConnectionDisconnectsPreviousHandler) {
  Signal<> signal;
  int firstCalls = 0;
  int secondCalls = 0;
  Connection connection = signal.Connect([&] { ++firstCalls; });
  connection = signal.Connect([&] { ++secondCalls; });

  signal.Emit();

  EXPECT_EQ(firstCalls, 0);
  EXPECT_EQ(secondCalls, 1);
}

TEST(SignalTest, HandlerCanDisconnectItselfWhileEmitting) {
  Signal<> signal;
  int calls = 0;
  Connection connection;
  connection = signal.Connect([&] {
    ++calls;
    connection.Disconnect();
  });

  signal.Emit();
  signal.Emit();

  EXPECT_EQ(calls, 1);
}

TEST(SignalTest, HandlerCanDisconnectALaterHandler) {
  Signal<> signal;
  int laterCalls = 0;
  Connection later;
  const Connection earlier = signal.Connect([&] { later.Disconnect(); });
  later = signal.Connect([&] { ++laterCalls; });

  signal.Emit();

  EXPECT_EQ(laterCalls, 0);
}

TEST(SignalTest, HandlerConnectedWhileEmittingRunsFromNextEmission) {
  Signal<> signal;
  int addedCalls = 0;
  std::optional<Connection> added;
  const Connection adder = signal.Connect([&] {
    if (!added.has_value()) {
      added = signal.Connect([&] { ++addedCalls; });
    }
  });

  signal.Emit();
  EXPECT_EQ(addedCalls, 0);
  signal.Emit();
  EXPECT_EQ(addedCalls, 1);
}

TEST(SignalTest, HandlerCanDestroyTheSignal) {
  auto signal = std::make_unique<Signal<>>();
  int laterCalls = 0;
  signal->Connect([&] { signal.reset(); }).Release();
  signal->Connect([&] { ++laterCalls; }).Release();

  signal->Emit();

  EXPECT_EQ(signal, nullptr);
  EXPECT_EQ(laterCalls, 0);
}

TEST(SignalTest, ConnectionOutlivingSignalIsSafe) {
  Connection connection;
  {
    Signal<> signal;
    connection = signal.Connect([] {});
    EXPECT_TRUE(connection.IsConnected());
  }
  EXPECT_FALSE(connection.IsConnected());
  connection.Disconnect();
}

TEST(SignalTest, ReentrantEmissionCallsHandlersAgain) {
  Signal<int> signal;
  std::vector<int> seen;
  const Connection connection = signal.Connect([&](int depth) {
    seen.push_back(depth);
    if (depth < 2) {
      signal.Emit(depth + 1);
    }
  });

  signal.Emit(0);

  EXPECT_EQ(seen, (std::vector<int>{0, 1, 2}));
}

TEST(SignalTest, PassesReferencesWithoutCopying) {
  Signal<const std::string&> signal;
  const std::string text{"shared"};
  const std::string* seen = nullptr;
  const Connection connection = signal.Connect([&](const std::string& value) { seen = &value; });

  signal.Emit(text);

  EXPECT_EQ(seen, &text);
}

TEST(SignalTest, MovedSignalKeepsHandlers) {
  Signal<> original;
  int calls = 0;
  const Connection connection = original.Connect([&] { ++calls; });
  Signal<> moved{std::move(original)};

  moved.Emit();

  EXPECT_EQ(calls, 1);
  EXPECT_TRUE(connection.IsConnected());
}

TEST(SignalTest, DisconnectAllStopsEveryHandler) {
  Signal<> signal;
  int calls = 0;
  const Connection first = signal.Connect([&] { ++calls; });
  const Connection second = signal.Connect([&] { ++calls; });

  signal.DisconnectAll();
  signal.Emit();

  EXPECT_EQ(calls, 0);
  EXPECT_FALSE(first.IsConnected());
}

TEST(ConnectionGroupTest, DisconnectsEveryConnectionTogether) {
  Signal<> signal;
  int calls = 0;
  ConnectionGroup group;
  group += signal.Connect([&] { ++calls; });
  group += signal.Connect([&] { ++calls; });
  EXPECT_EQ(group.GetCount(), 2U);

  signal.Emit();
  group.DisconnectAll();
  signal.Emit();

  EXPECT_EQ(calls, 2);
}

}  // namespace
}  // namespace flint::event
