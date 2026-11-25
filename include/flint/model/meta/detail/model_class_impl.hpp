#pragma once

// Reflection implementation of flint/model/model_class.hpp, which includes this file (only) when FLINT_HAS_REFLECTION
// is 1. clangd cannot parse this file until LLVM implements P2996; see flint ADR-0003.
//
// Only public members are bindable. GCC 16 notes, as in agate's class_info_impl.hpp: member access goes through a
// member-pointer template argument, and reflected values are bound to constexpr locals before run-time use.

#include <concepts>
#include <meta>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include "flint/model/model_class.hpp"

namespace flint::model::detail {

template <typename Type>
consteval auto PublicMembers() {
  return std::define_static_array(std::meta::nonstatic_data_members_of(^^Type, std::meta::access_context::unprivileged()));
}

/// Whether a type has members a view can bind to, directly or in nested models.
template <typename Type>
consteval auto IsModelType() -> bool {
  if constexpr (!std::is_class_v<Type> || std::is_union_v<Type>) {
    return false;
  } else {
    bool found = false;
    template for (constexpr auto kMember : PublicMembers<Type>()) {
      using Member = std::remove_cvref_t<typename[:std::meta::type_of(kMember):]>;
      if constexpr (kIsObservable<Member> || kIsCollection<Member> || std::same_as<Member, Command>) {
        found = true;
      } else if constexpr (IsModelType<Member>()) {
        found = true;
      }
    }
    return found;
  }
}

template <typename Type>
consteval auto TypeNameOf() -> std::string_view {
  return std::define_static_string(std::meta::display_string_of(std::meta::dealias(^^Type)));
}

template <typename Type>
auto BuildModelClass() -> ModelClass {
  std::vector<MemberInfo> members;
  if constexpr (std::is_class_v<Type> && !std::is_union_v<Type>) {
    template for (constexpr auto kMember : PublicMembers<Type>()) {
      using Member = std::remove_cvref_t<typename[:std::meta::type_of(kMember):]>;
      constexpr std::string_view kName = std::define_static_string(std::meta::identifier_of(kMember));
      constexpr auto kAddress = &MemberAddress<Type, &[:kMember:]>;
      if constexpr (kIsObservable<Member>) {
        using Value = Member::ValueType;
        constexpr std::string_view kTypeName = TypeNameOf<Value>();
        members.push_back(MemberInfo{
            .name = kName,
            .kind = MemberKind::Observable,
            .typeName = kTypeName,
            .valueType = &typeid(Value),
            .address = kAddress,
            .observable = &kObservableOperations<Value>,
        });
      } else if constexpr (std::same_as<Member, Command>) {
        members.push_back(MemberInfo{
            .name = kName,
            .kind = MemberKind::Command,
            .typeName = "flint::model::Command",
            .valueType = &typeid(Command),
            .address = kAddress,
        });
      } else if constexpr (kIsCollection<Member>) {
        using Item = Member::ItemType;
        constexpr std::string_view kTypeName = TypeNameOf<Item>();
        members.push_back(MemberInfo{
            .name = kName,
            .kind = MemberKind::Collection,
            .typeName = kTypeName,
            .valueType = &typeid(Member),
            .address = kAddress,
            .collection = &kCollectionOperations<Item>,
            .memberClass = &ModelClassOf<Item>,
        });
      } else if constexpr (IsModelType<Member>()) {
        constexpr std::string_view kTypeName = TypeNameOf<Member>();
        members.push_back(MemberInfo{
            .name = kName,
            .kind = MemberKind::Object,
            .typeName = kTypeName,
            .valueType = &typeid(Member),
            .address = kAddress,
            .memberClass = &ModelClassOf<Member>,
        });
      } else if constexpr (std::copy_constructible<Member>) {
        constexpr std::string_view kTypeName = TypeNameOf<Member>();
        members.push_back(MemberInfo{
            .name = kName,
            .kind = MemberKind::Value,
            .typeName = kTypeName,
            .valueType = &typeid(Member),
            .address = kAddress,
            .read = &CopyToAny<Member>,
        });
      }
    }
  }
  constexpr std::string_view kClassName = TypeNameOf<Type>();
  return ModelClass{kClassName, &typeid(Type), std::move(members), &CopyToAny<Type>};
}

}  // namespace flint::model::detail
