#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "flint/event/connection.hpp"
#include "flint/model/command.hpp"
#include "flint/model/observable.hpp"

namespace flint::model {
namespace {

struct PersonViewModel {
  Observable<std::string> Name{"Ada"};
  Observable<int> Age{36};
  Observable<std::vector<int>> Scores;
  int saves{0};
  Command Save{[this] { ++saves; }, [this] { return !Name->empty(); }};
};

TEST(ObservableTest, ReadsAsItsValue) {
  const PersonViewModel person;
  const std::string& name = person.Name;

  EXPECT_EQ(name, "Ada");
  EXPECT_EQ(person.Name.Get(), "Ada");
  EXPECT_EQ(*person.Age, 36);
  EXPECT_EQ(person.Name->size(), 3U);
}

TEST(ObservableTest, NotifiesWithTheNewValue) {
  PersonViewModel person;
  std::vector<std::string> seen;
  const event::Connection connection = person.Name.Changed.Connect([&](const std::string& name) { seen.push_back(name); });

  person.Name = "Grace";

  EXPECT_EQ(seen, (std::vector<std::string>{"Grace"}));
  EXPECT_EQ(person.Name.Get(), "Grace");
}

TEST(ObservableTest, AssigningTheSameValueDoesNotNotify) {
  PersonViewModel person;
  int calls = 0;
  const event::Connection connection = person.Age.Changed.Connect([&](int) { ++calls; });

  EXPECT_FALSE(person.Age.Set(36));
  person.Age = 36;

  EXPECT_EQ(calls, 0);
}

TEST(ObservableTest, NanIsTheSameAsNan) {
  Observable<float> width{std::numeric_limits<float>::quiet_NaN()};
  int calls = 0;
  const event::Connection connection = width.Changed.Connect([&](float) { ++calls; });

  width = std::numeric_limits<float>::quiet_NaN();
  width = 4.0F;

  EXPECT_EQ(calls, 1);
}

TEST(ObservableTest, ModifyChangesInPlaceAndNotifies) {
  PersonViewModel person;
  std::size_t seenSize = 0;
  const event::Connection connection = person.Scores.Changed.Connect([&](const std::vector<int>& scores) { seenSize = scores.size(); });

  person.Scores.Modify([](std::vector<int>& scores) { scores.push_back(10); });

  EXPECT_EQ(seenSize, 1U);
}

TEST(CommandTest, ExecutesWhenAvailable) {
  PersonViewModel person;

  EXPECT_TRUE(person.Save.CanExecute());
  EXPECT_TRUE(person.Save.Execute());
  EXPECT_EQ(person.saves, 1);
}

TEST(CommandTest, DoesNotExecuteWhenUnavailable) {
  PersonViewModel person;
  person.Name = "";

  EXPECT_FALSE(person.Save.CanExecute());
  EXPECT_FALSE(person.Save.Execute());
  EXPECT_EQ(person.saves, 0);
}

TEST(CommandTest, WithoutConditionIsAlwaysAvailable) {
  int runs = 0;
  Command command{[&] { ++runs; }};

  EXPECT_TRUE(command.Execute());
  EXPECT_EQ(runs, 1);
}

TEST(CommandTest, NotifiesWhenAvailabilityMayHaveChanged) {
  PersonViewModel person;
  int notifications = 0;
  const event::Connection connection = person.Save.CanExecuteChanged.Connect([&] { ++notifications; });
  person.Name.Changed.Connect([&](const std::string&) { person.Save.NotifyCanExecuteChanged(); }).Release();

  person.Name = "";

  EXPECT_EQ(notifications, 1);
}

}  // namespace
}  // namespace flint::model
