#pragma once

#include <algorithm>
#include <any>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

#include "flint/event/connection.hpp"
#include "flint/meta/reflection.hpp"
#include "flint/model/command.hpp"
#include "flint/model/observable.hpp"
#include "flint/model/observable_vector.hpp"

namespace flint::model {

class ModelClass;
struct MemberRef;

template <typename Type>
[[nodiscard]] auto ModelClassOf() -> const ModelClass&;

/// A model object and its reflected class. It does not own the object: whoever made the object keeps it alive while
/// anything refers to it (agate ADR-0023).
struct ObjectRef {
  void* object{nullptr};
  const ModelClass* type{nullptr};

  /// Refers to an object of any type.
  template <typename Object>
    requires(!std::is_const_v<Object>)
  [[nodiscard]] static auto To(Object& object) -> ObjectRef {
    return ObjectRef{.object = &object, .type = &ModelClassOf<Object>()};
  }

  [[nodiscard]] explicit operator bool() const noexcept { return object != nullptr; }

  /// Finds a member by name. Empty when there is no such member or no object.
  [[nodiscard]] auto Find(std::string_view name) const -> MemberRef;

  friend auto operator==(const ObjectRef& left, const ObjectRef& right) -> bool = default;
};

/// What a model member is, which decides how a view can use it.
enum class MemberKind : std::uint8_t {
  /// `Observable<T>`: read, written and watched.
  Observable,
  /// `Command`: invoked by a view.
  Command,
  /// `ObservableVector<T>`: items, and notifications when they are added or removed.
  Collection,
  /// A nested model (a class with bindable members): a path such as "Address.City" continues into it.
  Object,
  /// Any other public data member with a copyable type: read once, because nothing says when it changes.
  Value,
};

/// Type-erased access to an `Observable<T>`.
struct ObservableOperations {
  /// The current value. Empty when T cannot be copied.
  std::any (*get)(const void* observable){nullptr};
  /// Sets the value if `value` holds exactly T.
  /// @return Whether `value` had the right type.
  bool (*set)(void* observable, const std::any& value){nullptr};
  /// Calls `changed` after every change.
  event::Connection (*observe)(void* observable, std::function<void()> changed){nullptr};
};

/// What a view hears from a collection.
struct CollectionCallbacks {
  std::function<void(std::size_t)> inserted;
  std::function<void(std::size_t)> removing;
  std::function<void()> resetting;
  std::function<void()> reset;
};

/// Type-erased access to an `ObservableVector<T>`.
struct CollectionOperations {
  std::size_t (*size)(const void* collection){nullptr};
  /// The item at an index, as a model object.
  ObjectRef (*at)(void* collection, std::size_t index){nullptr};
  event::ConnectionGroup (*observe)(void* collection, CollectionCallbacks callbacks){nullptr};
};

/// A collection member, for views that show lists (agate's ItemsControl). It does not own the collection.
struct CollectionRef {
  void* collection{nullptr};
  const CollectionOperations* operations{nullptr};
  /// The class of the items.
  const ModelClass* itemClass{nullptr};

  [[nodiscard]] explicit operator bool() const noexcept { return collection != nullptr; }

  [[nodiscard]] auto Size() const -> std::size_t { return collection != nullptr ? operations->size(collection) : 0; }

  /// The item at an index, which must be less than Size().
  [[nodiscard]] auto At(std::size_t index) const -> ObjectRef { return operations->at(collection, index); }

  /// Subscribes to the collection's changes.
  [[nodiscard]] auto Observe(CollectionCallbacks callbacks) const -> event::ConnectionGroup {
    return collection != nullptr ? operations->observe(collection, std::move(callbacks)) : event::ConnectionGroup{};
  }

  friend auto operator==(const CollectionRef& left, const CollectionRef& right) -> bool = default;
};

/// One bindable member of a model class.
struct MemberInfo {
  std::string_view name;
  MemberKind kind{MemberKind::Value};
  /// The type a view sees, as written: T for `Observable<T>`, the item type for a collection, otherwise the member's
  /// own type.
  std::string_view typeName;
  /// The type a view sees: T for `Observable<T>`, otherwise the member's own type.
  const std::type_info* valueType{nullptr};
  /// The member's address within an object.
  void* (*address)(void* object){nullptr};
  /// Set for Observable members.
  const ObservableOperations* observable{nullptr};
  /// Set for Collection members.
  const CollectionOperations* collection{nullptr};
  /// The class of an Object member, or of a collection's items.
  const ModelClass& (*memberClass)(){nullptr};
  /// Copies a Value member.
  std::any (*read)(const void* member){nullptr};
};

/// A member found in an object: its address and description.
struct MemberRef {
  void* address{nullptr};
  const MemberInfo* info{nullptr};

  [[nodiscard]] explicit operator bool() const noexcept { return info != nullptr; }

  /// For an Object member, the nested object; otherwise an empty reference.
  [[nodiscard]] auto AsObject() const -> ObjectRef;

  /// For a Collection member, the collection; otherwise an empty reference.
  [[nodiscard]] auto AsCollection() const -> CollectionRef;
};

/// The bindable members of a type, found by C++26 reflection (flint ADR-0013). A view model is a plain class: its
/// public Observable, Command, ObservableVector and nested-model members are found by name, with no registration.
///
/// @code
/// struct PersonViewModel {
///   flint::model::Observable<std::string> Name{"Ada"};
///   flint::model::Command Save{[this] { ... }};
/// };
///
/// PersonViewModel person;
/// const flint::model::ObjectRef ref = flint::model::ObjectRef::To(person);
/// const flint::model::MemberRef name = ref.Find("Name");   // kind Observable, valueType std::string
/// @endcode
class ModelClass {
public:
  using CopyFunction = std::any (*)(const void* object);

  ModelClass() = default;

  ModelClass(std::string_view name, const std::type_info* type, std::vector<MemberInfo> members, CopyFunction copy)
      : m_name{name}, m_type{type}, m_members{std::move(members)}, m_copy{copy} {}

  [[nodiscard]] auto GetName() const noexcept -> std::string_view { return m_name; }

  [[nodiscard]] auto GetType() const noexcept -> const std::type_info* { return m_type; }

  [[nodiscard]] auto GetMembers() const noexcept -> const std::vector<MemberInfo>& { return m_members; }

  /// Finds a member's description by name.
  [[nodiscard]] auto FindMember(std::string_view name) const noexcept -> const MemberInfo* {
    for (const MemberInfo& member : m_members) {
      if (member.name == name) {
        return &member;
      }
    }
    return nullptr;
  }

  /// Copies an object of this class into a std::any, for a view that shows the object itself (`{Binding}`). Empty
  /// when the class cannot be copied.
  [[nodiscard]] auto Copy(const void* object) const -> std::any {
    return m_copy != nullptr && object != nullptr ? m_copy(object) : std::any{};
  }

  /// Whether the class has members a view can bind to.
  [[nodiscard]] auto IsModel() const noexcept -> bool {
    return std::ranges::any_of(m_members, [](const MemberInfo& member) { return member.kind != MemberKind::Value; });
  }

private:
  std::string_view m_name;
  const std::type_info* m_type{nullptr};
  std::vector<MemberInfo> m_members;
  CopyFunction m_copy{nullptr};
};

inline auto ObjectRef::Find(std::string_view name) const -> MemberRef {
  const MemberInfo* member = type != nullptr ? type->FindMember(name) : nullptr;
  if (member == nullptr || object == nullptr) {
    return {};
  }
  return MemberRef{.address = member->address(object), .info = member};
}

inline auto MemberRef::AsObject() const -> ObjectRef {
  if (info == nullptr || info->kind != MemberKind::Object) {
    return {};
  }
  return ObjectRef{.object = address, .type = &info->memberClass()};
}

inline auto MemberRef::AsCollection() const -> CollectionRef {
  if (info == nullptr || info->kind != MemberKind::Collection) {
    return {};
  }
  return CollectionRef{.collection = address, .operations = info->collection, .itemClass = &info->memberClass()};
}

namespace detail {

template <typename Type>
inline constexpr bool kIsObservable = false;
template <typename Value>
inline constexpr bool kIsObservable<Observable<Value>> = true;

template <typename Type>
inline constexpr bool kIsCollection = false;
template <typename Item>
inline constexpr bool kIsCollection<ObservableVector<Item>> = true;

template <typename Value>
auto CopyToAny(const void* value) -> std::any {
  if constexpr (std::copy_constructible<Value>) {
    return *static_cast<const Value*>(value);
  } else {
    return {};
  }
}

template <typename Value>
inline constexpr ObservableOperations kObservableOperations{
    .get = [](const void* observable) -> std::any {
      if constexpr (std::copy_constructible<Value>) {
        return static_cast<const Observable<Value>*>(observable)->Get();
      } else {
        return {};
      }
    },
    .set = [](void* observable, const std::any& value) -> bool {
      const auto* typed = std::any_cast<Value>(&value);
      if constexpr (std::copy_constructible<Value>) {
        if (typed != nullptr) {
          static_cast<Observable<Value>*>(observable)->Set(*typed);
        }
      }
      return typed != nullptr;
    },
    .observe = [](void* observable, std::function<void()> changed) -> event::Connection {
      return static_cast<Observable<Value>*>(observable)->Changed.Connect([changed = std::move(changed)](const Value&) { changed(); });
    },
};

template <typename Item>
inline constexpr CollectionOperations kCollectionOperations{
    .size = [](const void* collection) -> std::size_t { return static_cast<const ObservableVector<Item>*>(collection)->Size(); },
    .at = [](void* collection, std::size_t index) -> ObjectRef {
      return ObjectRef::To((*static_cast<ObservableVector<Item>*>(collection))[index]);
    },
    .observe = [](void* collection, CollectionCallbacks callbacks) -> event::ConnectionGroup {
      auto& vector = *static_cast<ObservableVector<Item>*>(collection);
      event::ConnectionGroup connections;
      connections += vector.Inserted.Connect(std::move(callbacks.inserted));
      connections += vector.Removing.Connect(std::move(callbacks.removing));
      connections += vector.Resetting.Connect(std::move(callbacks.resetting));
      connections += vector.Reset.Connect(std::move(callbacks.reset));
      return connections;
    },
};

template <typename Owner, auto Member>
auto MemberAddress(void* object) -> void* {
  return &(static_cast<Owner*>(object)->*Member);
}

/// Builds a class's ModelClass by reflecting over its public data members.
template <typename Type>
[[nodiscard]] auto BuildModelClass() -> ModelClass;

}  // namespace detail

/// Returns the ModelClass of a type, built by reflection on first use. Thread-safe.
template <typename Type>
auto ModelClassOf() -> const ModelClass& {
  static const ModelClass kClass = detail::BuildModelClass<std::remove_cv_t<Type>>();
  return kClass;
}

}  // namespace flint::model

#if FLINT_HAS_REFLECTION
#include "flint/model/meta/detail/model_class_impl.hpp"
#else
// Tooling-only stub for clangd and clang-tidy, which cannot parse reflection. Never compiled into a binary: see
// flint/meta/reflection.hpp.
namespace flint::model::detail {

template <typename Type>
auto BuildModelClass() -> ModelClass {
  return ModelClass{};
}

}  // namespace flint::model::detail
#endif
