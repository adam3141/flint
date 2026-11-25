#include "flint/model/model_class.hpp"

#include <any>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "flint/event/connection.hpp"
#include "flint/model/command.hpp"
#include "flint/model/observable.hpp"
#include "flint/model/observable_vector.hpp"

namespace flint::model {
namespace {

struct AddressViewModel {
  Observable<std::string> City{"London"};
};

struct ItemViewModel {
  explicit ItemViewModel(std::string title) : Title{std::move(title)} {}

  Observable<std::string> Title;
};

class PersonViewModel {
public:
  Observable<std::string> Name{"Ada"};
  Observable<int> Age{36};
  AddressViewModel Address;
  ObservableVector<ItemViewModel> Items;
  Command Greet{[this] { ++greetings; }};
  int greetings{0};

  [[nodiscard]] auto GetHidden() const -> int { return m_hidden; }

private:
  int m_hidden{0};
};

auto KindOf(const ModelClass& type, std::string_view name) -> std::optional<MemberKind> {
  const MemberInfo* member = type.FindMember(name);
  return member != nullptr ? std::optional{member->kind} : std::nullopt;
}

TEST(ModelClassTest, FindsPublicMembersByKind) {
  const ModelClass& type = ModelClassOf<PersonViewModel>();
  EXPECT_TRUE(type.IsModel());
  const MemberInfo* name = type.FindMember("Name");
  const MemberInfo* age = type.FindMember("Age");
  ASSERT_NE(name, nullptr);
  ASSERT_NE(age, nullptr);
  EXPECT_EQ(name->kind, MemberKind::Observable);
  ASSERT_NE(name->valueType, nullptr);
  EXPECT_EQ(*name->valueType, typeid(std::string));
  EXPECT_EQ(age->typeName, "int");
  EXPECT_EQ(KindOf(type, "Address"), MemberKind::Object);
  EXPECT_EQ(KindOf(type, "Items"), MemberKind::Collection);
  EXPECT_EQ(KindOf(type, "Greet"), MemberKind::Command);
  EXPECT_EQ(KindOf(type, "greetings"), MemberKind::Value);
  EXPECT_EQ(KindOf(type, "m_hidden"), std::nullopt);
  EXPECT_FALSE(ModelClassOf<std::string>().IsModel());
}

TEST(ModelClassTest, ReadsWritesAndWatchesObservables) {
  PersonViewModel person;
  const MemberRef name = ObjectRef::To(person).Find("Name");
  ASSERT_TRUE(name);
  const ObservableOperations& operations = *name.info->observable;
  EXPECT_EQ(std::any_cast<std::string>(operations.get(name.address)), "Ada");

  int changes = 0;
  const event::Connection connection = operations.observe(name.address, [&changes] { ++changes; });
  EXPECT_TRUE(operations.set(name.address, std::string{"Grace"}));
  EXPECT_FALSE(operations.set(name.address, 42));
  EXPECT_EQ(person.Name.Get(), "Grace");
  EXPECT_EQ(changes, 1);
}

TEST(ModelClassTest, FollowsNestedModelsAndCommands) {
  PersonViewModel person;
  const ObjectRef address = ObjectRef::To(person).Find("Address").AsObject();
  ASSERT_TRUE(address);
  const MemberRef city = address.Find("City");
  ASSERT_TRUE(city);
  EXPECT_EQ(std::any_cast<std::string>(city.info->observable->get(city.address)), "London");

  const MemberRef greet = ObjectRef::To(person).Find("Greet");
  static_cast<Command*>(greet.address)->Execute();
  EXPECT_EQ(person.greetings, 1);
  const MemberRef greetings = ObjectRef::To(person).Find("greetings");
  EXPECT_EQ(std::any_cast<int>(greetings.info->read(greetings.address)), 1);
  EXPECT_FALSE(ObjectRef::To(person).Find("Missing"));
}

TEST(ModelClassTest, ExposesCollectionsAndTheirChanges) {
  PersonViewModel person;
  person.Items.Emplace("first");
  const MemberRef items = ObjectRef::To(person).Find("Items");
  const CollectionOperations& operations = *items.info->collection;
  EXPECT_EQ(&items.info->memberClass(), &ModelClassOf<ItemViewModel>());

  std::vector<std::string> events;
  const event::ConnectionGroup connections =
      operations.observe(items.address,
                         CollectionCallbacks{
                             .inserted = [&events](std::size_t index) { events.push_back("inserted " + std::to_string(index)); },
                             .removing =
                                 [&events, &person](std::size_t index) {
                                   // The item still exists while views let go of it.
                                   events.push_back("removing " + person.Items[index].Title.Get());
                                 },
                             .resetting = [&events] { events.emplace_back("resetting"); },
                             .reset = [&events] { events.emplace_back("reset"); },
                         });
  person.Items.EmplaceAt(0, "zeroth");
  person.Items.Erase(1);
  person.Items.Clear();
  EXPECT_EQ(events, (std::vector<std::string>{"inserted 0", "removing first", "resetting", "reset"}));

  person.Items.Emplace("again");
  EXPECT_EQ(operations.size(items.address), 1U);
  const CollectionRef collection = items.AsCollection();
  EXPECT_EQ(collection.Size(), 1U);
  EXPECT_EQ(collection.itemClass, &ModelClassOf<ItemViewModel>());
  EXPECT_FALSE(ObjectRef::To(person).Find("Name").AsCollection());
  const ObjectRef item = operations.at(items.address, 0);
  EXPECT_EQ(item.type, &ModelClassOf<ItemViewModel>());
  EXPECT_EQ(std::any_cast<std::string>(item.Find("Title").info->observable->get(item.Find("Title").address)), "again");
}

TEST(ObservableVectorTest, KeepsItemsInPlaceAndIterates) {
  ObservableVector<ItemViewModel> items;
  const ItemViewModel& first = items.Emplace("a");
  items.Emplace("b");
  items.EmplaceAt(0, "c");
  EXPECT_EQ(&items[1], &first);
  std::string titles;
  for (const ItemViewModel& item : items) {
    titles += item.Title.Get();
  }
  EXPECT_EQ(titles, "cab");
}

}  // namespace
}  // namespace flint::model
