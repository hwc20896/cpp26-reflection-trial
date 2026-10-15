#pragma once

//  Annotation basics for C++26 reflection (P3394).
//  An annotation wraps a compile-time value into a declaration via [[=expr]];
//  std::meta::annotations_of_with_type reads it back at compile time.
//  This file supplies the generic queries plus a ready-to-use string annotation.

#include <meta>
#include <string_view>

//  Shorthand for annotation syntax: ANNOTATION(x) == [[=x]].
//  Keeps the attachment sites readable when the annotation type is verbose.
#define ANNOTATION(x) [[=x]]

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
    //  Type-level query: does T carry an annotation of type Anno?
    //  Usable inside concepts (e.g., the `x_derived` concepts emitted by the DEFINE_ANNO_* macros).
    template <class T, class Anno>
    constexpr bool has_annotation = !std::meta::annotations_of_with_type(^^T, ^^Anno).empty();

    //  Entity-level query: does this reflection (a member, an enumerator, ...)
    //  carry an annotation of type Anno? For use inside `template for` loops.
    template <class Anno>
    consteval bool has_annotation_of(const std::meta::info entity) {
        return !std::meta::annotations_of_with_type(entity, ^^Anno).empty();
    }
}

//  An example of string annotation.
//  Constraints:
//  - Only public fields may be present in the class used in annotation. Therefore neither std::string nor std::string_view may be used.
//  - For const char* member, `std::define_static_string` must be used to initialize it.
struct StrAnnotation {
    const char* value;
    explicit consteval StrAnnotation(std::string_view _value)
        : value(std::define_static_string(_value)){}
};

enum class StrEnumExample {
    a ANNOTATION(StrAnnotation("Alpha")),
    b ANNOTATION(StrAnnotation("Bravo")),
    c ANNOTATION(StrAnnotation("Charlie")),
    //  etc.
};

