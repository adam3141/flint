#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include "flint/event/signal.hpp"

namespace flint::model {

/// A list that notifies when items are added or removed: what agate's ItemsControl binds to (flint ADR-0013).
///
/// Items are created in place and never move in memory, so they can be view models made of Observables (which cannot
/// be copied or moved), and views can refer to them while they are in the list.
///
/// Not thread-safe: change it on the thread that observes it (for agate, the UI thread).
///
/// @code
/// struct PersonViewModel {
///   explicit PersonViewModel(std::string name) : Name{std::move(name)} {}
///   flint::model::Observable<std::string> Name;
/// };
///
/// flint::model::ObservableVector<PersonViewModel> people;
/// people.Emplace("Ada");            // Inserted(0)
/// people.EmplaceAt(0, "Grace");     // Inserted(0)
/// people.Erase(1);                  // Removing(1), then the item is destroyed
/// for (PersonViewModel& person : people) { ... }
/// @endcode
template <typename Item>
class ObservableVector {
public:
  using ItemType = Item;

  /// Iterates over items rather than over their storage.
  template <typename Value, typename Base>
  class BasicIterator {
  public:
    using iterator_category = std::random_access_iterator_tag;  // NOLINT(readability-identifier-naming): standard name
    using value_type = std::remove_const_t<Value>;              // NOLINT(readability-identifier-naming): standard name
    using difference_type = std::ptrdiff_t;                     // NOLINT(readability-identifier-naming): standard name
    using pointer = Value*;                                     // NOLINT(readability-identifier-naming): standard name
    using reference = Value&;                                   // NOLINT(readability-identifier-naming): standard name

    BasicIterator() = default;

    explicit BasicIterator(Base base) : m_base{base} {}

    [[nodiscard]] auto operator*() const -> Value& { return **m_base; }

    [[nodiscard]] auto operator->() const -> Value* { return m_base->get(); }

    [[nodiscard]] auto operator[](difference_type offset) const -> Value& { return *m_base[offset]; }

    auto operator++() -> BasicIterator& {
      ++m_base;
      return *this;
    }

    auto operator++(int) -> BasicIterator { return BasicIterator{m_base++}; }

    auto operator--() -> BasicIterator& {
      --m_base;
      return *this;
    }

    auto operator--(int) -> BasicIterator { return BasicIterator{m_base--}; }

    auto operator+=(difference_type offset) -> BasicIterator& {
      m_base += offset;
      return *this;
    }

    auto operator-=(difference_type offset) -> BasicIterator& {
      m_base -= offset;
      return *this;
    }

    [[nodiscard]] friend auto operator+(BasicIterator iterator, difference_type offset) -> BasicIterator { return iterator += offset; }

    [[nodiscard]] friend auto operator+(difference_type offset, BasicIterator iterator) -> BasicIterator { return iterator += offset; }

    [[nodiscard]] friend auto operator-(BasicIterator iterator, difference_type offset) -> BasicIterator { return iterator -= offset; }

    [[nodiscard]] friend auto operator-(const BasicIterator& left, const BasicIterator& right) -> difference_type {
      return left.m_base - right.m_base;
    }

    [[nodiscard]] friend auto operator==(const BasicIterator& left, const BasicIterator& right) -> bool = default;
    [[nodiscard]] friend auto operator<=>(const BasicIterator& left, const BasicIterator& right) = default;

  private:
    Base m_base{};
  };

  using Storage = std::vector<std::unique_ptr<Item>>;
  using iterator = BasicIterator<Item, typename Storage::iterator>;                    // NOLINT(readability-identifier-naming)
  using const_iterator = BasicIterator<const Item, typename Storage::const_iterator>;  // NOLINT(readability-identifier-naming)

  ObservableVector() = default;
  ~ObservableVector() = default;

  ObservableVector(const ObservableVector&) = delete;
  auto operator=(const ObservableVector&) -> ObservableVector& = delete;
  ObservableVector(ObservableVector&&) = delete;
  auto operator=(ObservableVector&&) -> ObservableVector& = delete;

  [[nodiscard]] auto Size() const noexcept -> std::size_t { return m_items.size(); }

  [[nodiscard]] auto IsEmpty() const noexcept -> bool { return m_items.empty(); }

  /// The item at an index, which must be less than Size().
  [[nodiscard]] auto operator[](std::size_t index) -> Item& { return *m_items[index]; }

  [[nodiscard]] auto operator[](std::size_t index) const -> const Item& { return *m_items[index]; }

  [[nodiscard]] auto begin() -> iterator { return iterator{m_items.begin()}; }  // NOLINT(readability-identifier-naming)

  [[nodiscard]] auto end() -> iterator { return iterator{m_items.end()}; }  // NOLINT(readability-identifier-naming)

  [[nodiscard]] auto begin() const -> const_iterator {  // NOLINT(readability-identifier-naming)
    return const_iterator{m_items.begin()};
  }

  [[nodiscard]] auto end() const -> const_iterator { return const_iterator{m_items.end()}; }  // NOLINT(readability-identifier-naming)

  /// Creates an item at the end, then emits Inserted.
  template <typename... Arguments>
  auto Emplace(Arguments&&... arguments) -> Item& {
    return EmplaceAt(m_items.size(), std::forward<Arguments>(arguments)...);
  }

  /// Creates an item before `index` (at most Size()), then emits Inserted.
  template <typename... Arguments>
  auto EmplaceAt(std::size_t index, Arguments&&... arguments) -> Item& {
    const auto position = m_items.begin() + static_cast<std::ptrdiff_t>(index);
    Item& item = **m_items.insert(position, std::make_unique<Item>(std::forward<Arguments>(arguments)...));
    Inserted.Emit(index);
    return item;
  }

  /// Emits Removing, then destroys the item at `index` (less than Size()).
  auto Erase(std::size_t index) -> void {
    Removing.Emit(index);
    m_items.erase(m_items.begin() + static_cast<std::ptrdiff_t>(index));
  }

  /// Emits Resetting, destroys every item, then emits Reset.
  auto Clear() -> void {
    Resetting.Emit();
    m_items.clear();
    Reset.Emit();
  }

  /// Emitted after an item is created, with its index.
  event::Signal<std::size_t> Inserted;
  /// Emitted before an item is destroyed, with its index, so views can let go of it first.
  event::Signal<std::size_t> Removing;
  /// Emitted before every item is destroyed at once.
  event::Signal<> Resetting;
  /// Emitted after the list was cleared; views rebuild from the current items.
  event::Signal<> Reset;

private:
  Storage m_items;
};

}  // namespace flint::model
