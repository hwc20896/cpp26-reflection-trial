#pragma once

#include <meta>

#include <array>
#include <functional>
#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "reflection-util.hpp"

//  This file only consists reflection methods about ENUMS.
//  Enum-flavored facade over the generic walkers in reflection-util.hpp:
//  each query is a variable template sized by enum_size, paired with lookup
//  functions (to_index, enum_from_name) built on top of those arrays.

namespace detail {
    //  True when the enumerators are 0, 1, 2, ... in declaration order
    //  (the fast-path condition for to_index).
    template <is_enum E>
    constexpr bool is_sequential_zero_based() noexcept {
        constexpr auto values = detail::metadata_array<E>(&std::meta::extract<E>);
        using Underlying = std::underlying_type_t<E>;

        return std::ranges::equal(
            values, std::views::iota(Underlying(0), static_cast<Underlying>(values.size())),
            {}, [](E e) { return static_cast<Underlying>(e); }
        );
    }
}

//  Number of enumerators in the enum.
template <detail::is_enum E>
inline constexpr std::size_t enum_size = detail::member_size<E>;

//  Get the value array of the enum.
//  e.g., `enum class Foo {a = 0, b = 1, c = 2};` yields [Foo::a, Foo::b, Foo::c].
//  After then, you may cast to its underlying type to retrieve its values.
template <detail::is_enum E>
inline constexpr std::array<E, enum_size<E>> enum_values = detail::metadata_array<E>(&std::meta::extract<E>);

//  Get the name of each values in the enum.
//  e.g., `enum class Foo {a = 0, b = 1, c = 2};` yields ["a", "b", "c"].
template <detail::is_enum E>
inline constexpr std::array<std::string_view, enum_size<E>> enum_names = detail::metadata_array<E>(&std::meta::identifier_of);

//  Get the full name, including the enum name, of each values in the enum.
//  e.g., `enum class Foo {a = 0, b = 1, c = 2};` yields ["Foo::a", "Foo::b", "Foo::c"].
template <detail::is_enum E>
inline constexpr std::array<std::string_view, enum_size<E>> enum_full_names = detail::metadata_array<E>(&std::meta::display_string_of);

//  (identifier, value) pair for each enumerator, in declaration order.
//  e.g., `enum class Foo {a = 3, b = 8};` yields [{"a", Foo::a}, {"b", Foo::b}].
template <detail::is_enum E>
inline constexpr std::array<std::tuple<std::string_view, E>, enum_size<E>> enum_name_value_pairs
    = detail::metadata_multiple_array<E>(&std::meta::identifier_of, &std::meta::extract<E>);

//  Index of `e` in declaration order.
//  Fast path: for sequential zero-based enums the underlying value is the index,
//  range-checked against [0, enum_size<E>); otherwise a linear search runs.
//  Throws std::out_of_range if `e` is not an enumerator (e.g. a wild value).
template <detail::is_enum E>
constexpr std::size_t to_index(E e) {
    if constexpr (detail::is_sequential_zero_based<E>()) {
        if (const auto v = std::to_underlying(e);
            std::cmp_greater_equal(v, 0) && std::cmp_less(v, enum_size<E>)
        )
            return static_cast<std::size_t>(v);
    }
    else {
        for (std::size_t i = 0; i < enum_size<E>; ++i) {
            if (enum_values<E>[i] == e) {
                return i;
            }
        }
    }
    throw std::out_of_range("Enum value not found");
}

//  Inverse of `enum_names`: map an identifier back to its enumerator value.
//  Throws std::out_of_range if no enumerator matches.
template <detail::is_enum E>
constexpr E enum_from_name(const std::string_view str) {
    for (const auto& [name, val] : enum_name_value_pairs<E>) {
        if (name == str) {
            return val;
        }
    }
    throw std::out_of_range("Enum value not found");
}