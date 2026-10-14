#pragma once

//  Generic reflection primitives shared by the enum- and class-flavored layers.
//  Mechanism, not policy: everything here works on any type with `has_members`,
//  covering enums (walk the enumerators) and records (walk the nonstatic data
//  members), and projects each member reflection into a compile-time array.

#include <meta>
#include <string_view>
#include <type_traits>
#include <array>
#include <algorithm>
#include <functional>

namespace detail {
    //  Enum type, after stripping references and cv-qualifiers.
    template <class E>
    concept is_enum = std::is_enum_v<std::remove_cvref_t<E>>;

    //  Record type (class, struct, or union): a non-enum that has data members.
    template <class T>
    concept nonenum_class = std::is_class_v<std::remove_cvref_t<T>> || std::is_union_v<std::remove_cvref_t<T>>;

    //  Anything this file can walk. Gate for every walker below.
    template <class T>
    concept has_members = is_enum<T> || nonenum_class<T>;

    //  Unqualified name of T, e.g., "Foo" for `struct Foo`.
    template <class T>
    inline constexpr std::string_view class_name = std::meta::identifier_of(^^T);

    //  String-like types whose Debug form is quoted and escaped.
    template <class T>
    inline constexpr bool is_debug_string =
        std::same_as<std::remove_cvref_t<T>, std::string> ||
        std::same_as<std::remove_cvref_t<T>, std::string_view>;

    //  Number of enumerators (enum) or nonstatic data members (record).
    //  `if constexpr` discards the untaken branch, so only the query that is
    //  meaningful for T is ever instantiated.
    template <class T>
    requires has_members<T>
    constexpr std::size_t _member_size() {
        if constexpr (is_enum<T>)
            return std::meta::enumerators_of(^^T).size();
        else
            return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
    }

    //  Same count, exposed as a variable template so it can size std::array.
    template <class T>
    requires has_members<T>
    constexpr std::size_t member_size = _member_size<T>();

    //  Apply `proj` to every member reflection of T, in declaration order:
    //  element i = proj(reflection of the i-th enumerator / data member).
    //  `proj` takes a std::meta::info and returns the desired payload
    //  (an identifier, an extracted value, a display name, ...).
    template <class T, class Proj>
    requires has_members<T>
    constexpr auto metadata_array(Proj&& proj) -> std::array<std::invoke_result_t<decltype(proj), std::meta::info>, member_size<T>> {
        using result_t = std::invoke_result_t<decltype(proj), std::meta::info>;
        std::array<result_t, member_size<T>> arr;
        if constexpr (is_enum<T>) {
            std::ranges::transform(std::meta::enumerators_of(^^T), arr.begin(), std::forward<Proj>(proj));
        }
        else {
            std::ranges::transform(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()), arr.begin(), std::forward<Proj>(proj));
        }
        return arr;
    }

    //  Tuple-flavored walker: element i = (projs(reflection_i)...).
    //  E.g., project each enumerator into both its identifier and its value.
    template <class T, class... Projs>
    requires has_members<T>
    constexpr auto metadata_multiple_array(Projs&&... projs) -> std::array<std::tuple<std::invoke_result_t<decltype(projs), std::meta::info>...>, member_size<T>> {
        return metadata_array<T>([&projs...](const std::meta::info i) {
            return std::make_tuple(std::invoke(projs, i)...);
        });
    }


}
