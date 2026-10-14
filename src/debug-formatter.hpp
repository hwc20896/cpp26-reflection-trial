#pragma once

//  derive(Debug) for C++26 reflection: Debug-style std::formatter support.
//
//  The whole pattern, end to end:
//    [[= derive::debug]] annotation on a type  -> the switch
//    `debug_derived<T>` concept                -> the detection
//    constrained std::formatter<T> partial     -> the implementation
//  No per-type code is generated: the formatters reflect over T and walk
//  its members at compile time.
//
//  Usage:
//    enum class ANNOTATION(derive::debug) E { ... };
//    struct   ANNOTATION(derive::debug) S { ... };
//    std::println("{}", value);   // E -> `E::first`, S -> `S { field: ... }`
//
//  Output aims to match Rust's {:?}: fully qualified enum names, quoted and
//  escaped strings/chars, and per-member debug_skip / debug_rename support.

#include <meta>

#include <format>
#include "annotations.hpp"
#include "enum-reflections.hpp"
#include "reflection-util.hpp"

//  Declare a payload-less tag annotation plus its detection concept `x_derived`.
//  Attach with ANNOTATION(x); test with x_derived<T>.
#define DEFINE_ANNO_TAG(x)\
    inline constexpr struct _##x##_t final {} x;\
    template <class T>\
    concept x##_derived = detail::has_annotation<T, _##x##_t>;

//  Declare an annotation class with payload plus its detection concept.
//  Follow the invocation with the class body (and a trailing `;`), e.g.:
//    DEFINE_ANNO_CLASS(rename) { const char* rename_to; ... };
#define DEFINE_ANNO_CLASS(x)\
    struct x;\
    template <class T>\
    concept x##_derived = detail::has_annotation<T, x>;\
    struct x

namespace detail {
    //  Write one char in Rust Debug escape style: `\\`, `\n`, `\r`, `\t`,
    //  `\0`, the `quote` character itself, and `\u{XX}` for other control
    //  characters. `quote` is the surrounding delimiter of the context:
    //  '\"' when inside a string, '\'' when inside a character.
    template <class OutputIt>
    constexpr OutputIt write_escaped_char(OutputIt out, const char ch, const char quote) {
        switch (ch) {
        case '\\': return std::format_to(out, "\\\\");
        case '\n': return std::format_to(out, "\\n");
        case '\r': return std::format_to(out, "\\r");
        case '\t': return std::format_to(out, "\\t");
        case '\0': return std::format_to(out, "\\0");
        default: break;
        }
        if (ch == quote)
            return std::format_to(out, "\\{}", ch);
        if (const auto byte = static_cast<unsigned char>(ch); byte < 0x20 || byte == 0x7F)
            return std::format_to(out, "\\u{{{:x}}}", byte);
        *out++ = ch;
        return out;
    }

    //  Escape every char of `str` for a double-quoted string context.
    template <class OutputIt>
    constexpr OutputIt write_escaped_string(OutputIt out, const std::string_view str) {
        for (const char ch : str)
            out = write_escaped_char(out, ch, '"');
        return out;
    }
}

namespace derive {
    //  Turn Debug on for a type: attach ANNOTATION(derive::debug).
    DEFINE_ANNO_TAG(debug)

    //  Member annotation: omit this field from Debug output.
    DEFINE_ANNO_TAG(debug_skip)

    //  Member annotation: print this field under a different label, e.g.,
    //  ANNOTATION(derive::debug_rename("message")).
    DEFINE_ANNO_CLASS(debug_rename) {
        const char* rename_to;
        explicit consteval debug_rename(std::string_view str)
            : rename_to(std::define_static_string(str)) {}
    };

    //  Label to print for a member: debug_rename payload if present,
    //  otherwise the member's own identifier.
    consteval std::string_view member_label(const std::meta::info member) {
        if (const auto annos = std::meta::annotations_of_with_type(member, ^^debug_rename); !annos.empty())
            return std::meta::extract<debug_rename>(annos.front()).rename_to;
        return std::meta::identifier_of(member);
    }
}

//  Debug formatter for annotated enums: prints the fully qualified
//  enumerator name (e.g., `Test::A`). No format spec is supported; `{}` only.
template <class T>
requires derive::debug_derived<T> && detail::is_enum<T>
struct std::formatter<T> {
    constexpr auto parse(auto& ctx) {
        return ctx.begin();
    }
    constexpr auto format(const T& t, auto& ctx) const {
        return std::format_to(ctx.out(), "{}", enum_full_names<T>[to_index(t)]);
    }
};

//  Debug formatter for annotated records: `Name { field: value, ... }`.
//  Members are reflected once into a static array, then walked with
//  `template for`:
//    - debug_skip members are omitted;
//    - string-like members are quoted and escaped;
//    - anything else falls through to `{}`, which recurses into nested
//      Debug formatters (or built-in / range formatting).
//  No format spec is supported; `{}` only.
template <class T>
requires derive::debug_derived<T> && detail::nonenum_class<T>
struct std::formatter<T> {
    constexpr auto parse(auto& ctx) {
        return ctx.begin();
    }
    constexpr auto format(const T& t, auto& ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "{} {{ ", detail::class_name<T>);

        static constexpr auto members = std::define_static_array(
            std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())
        );

        bool first = true;
        template for (constexpr auto member : members) {
            if constexpr (!detail::has_annotation_of<derive::_debug_skip_t>(member)) {
                if (!first)
                    out = std::format_to(out, ", ");
                first = false;

                using MemberType = [:std::meta::type_of(member):];

                if constexpr (detail::is_debug_string<MemberType>) {
                    out = std::format_to(out, "{}: \"", derive::member_label(member));
                    out = detail::write_escaped_string(out, t.[:member:]);
                    out = std::format_to(out, "\"");
                }
                else if constexpr (std::is_same_v<std::remove_cvref_t<MemberType>, char>) {
                    out = std::format_to(out, "{}: \'", derive::member_label(member));
                    out = detail::write_escaped_char(out, t.[:member:], '\'');
                    out = std::format_to(out, "\'");
                }
                else {
                    out = std::format_to(out, "{}: {}", derive::member_label(member), t.[:member:]);
                }
            }
        }

        return std::format_to(out, " }}");
    }
};
