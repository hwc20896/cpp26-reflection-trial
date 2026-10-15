#pragma once

//  derive(JSON) for C++26 reflection: serde-style to_json / from_json.
//
//  The whole pattern, end to end:
//    [[= derive::json]] annotation on a type   -> the switch
//    `json_derived<T>` concept                 -> the detection
//    constrained write_json / parse_json chain -> the implementation
//  No per-type code is generated: both chains reflect over T and walk its
//  members at compile time. Debug-only annotations (debug_skip,
//  debug_rename) deliberately do not apply here.
//
//  Usage:
//    struct ANNOTATION(derive::json) S { ... };
//    std::string s = to_json(value);      //  compact, one line
//    std::string p = to_json(value, 2);   //  indented, 2 spaces per level
//    S back = from_json<S>(s);            //  compact or pretty text alike

#include <meta>
#include <string>
#include <format>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <charconv>
#include <limits>
#include <utility>

#include "reflection-util.hpp"
#include "annotations.hpp"
#include "enum-reflections.hpp"

namespace derive {
    DEFINE_ANNO_TAG(json)
}

namespace detail {
    template <class T>
    concept json_object = derive::json_derived<T> && detail::nonenum_class<T>;

    template <class T>
    concept json_enum = derive::json_derived<T> && detail::is_enum<T>;

    //  Pretty-print state threaded through the entire write chain.
    //  step == 0 is the compact fast path (every gate below stays off, so
    //  the output is byte-identical to a one-line write); otherwise an item
    //  at `level` is indented level * step spaces, and inner() yields the
    //  format for the next nesting level.
    template <class OutputIt>
    class json_format {
        public:
            explicit constexpr json_format(OutputIt out, const std::size_t step = 0)
                : out_(out), step_(step) {}

            template <class T>
            constexpr void write_json(const T& value) {
                using U = std::remove_cvref_t<T>;

                if constexpr (std::is_same_v<U, char>) {
                    write_string(std::string_view(&value, 1));
                }
                else if constexpr (std::integral<U>) {
                    out_ = std::format_to(out_, "{}", value);
                }
                else if constexpr (std::floating_point<U>) {
                    //  JSON has no inf / nan; emit null like JSON.stringify does.
                    if (!std::isfinite(value)) {
                        out_ = std::format_to(out_, "null");
                    }
                    else {
                        out_ = std::format_to(out_, "{}", value);
                    }
                }
                else if constexpr (is_debug_string<U>) {
                    write_string(value);
                }
                else if constexpr (json_enum<U>) {
                    write_enum(value);
                }
                else if constexpr (json_object<U>) {
                    write_record(value);
                }
                else if constexpr (std::ranges::input_range<U>) {
                    *out_++ = '[';
                    level_++;
                    bool first = true;
                    for (const auto& elem : value) {
                        if (!first) {
                            *out_++ = ',';
                        }
                        if (pretty()) {
                            write_newline_indent(width());
                        }
                        first = false;
                        write_json(elem);
                    }
                    level_--;
                    if (!first && pretty()) {
                        write_newline_indent(width());
                    }
                    *out_++ = ']';
                }
                else {
                    //  C++26 P2593: a bare static_assert(false) is allowed in templates
                    //  and only fires when this branch is actually instantiated.
                    static_assert(false, "to_json: no JSON mapping for this type -- attach ANNOTATION(derive::json) to the struct or enum");
                }
            }
        private:
            OutputIt out_;

            std::size_t step_ = 0;
            std::size_t level_ = 0;

            [[nodiscard]]
            constexpr bool pretty() const {
                return step_ != 0;
            }

            [[nodiscard]]
            constexpr std::size_t width() const {
                return level_ * step_;
            }

            constexpr void write_newline_indent(const std::size_t width) {
                *out_++ = '\n';
                out_ = std::fill_n(out_, width, ' ');
            }

            //  Annotated record -> `{"field":value,...}`. Every nonstatic data member
            //  is emitted under its own identifier; Debug's debug_skip / debug_rename
            //  deliberately do not apply here (they are Debug-only annotations).
            template <class T>
            requires json_object<T>
            constexpr void write_record(const T& value) {
                static constexpr auto members = std::define_static_array(
                    std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())
                );

                *out_++ = '{';
                level_++;
                bool first = true;
                template for (constexpr auto member : members) {
                    if (!first)
                        *out_++ = ',';
                    if (pretty()) {
                        write_newline_indent(width());
                    }
                    first = false;

                    write_string(std::meta::identifier_of(member));
                    *out_++ = ':';
                    if (pretty()) {
                        *out_++ = ' ';
                    }
                    write_json(value.[:member:]);
                }
                level_--;
                if (!first && pretty()) {
                    write_newline_indent(width());
                }
                *out_++ = '}';
            }

            //  Annotated enum -> its bare enumerator name, serde-style: `"A"`.
            //  Round-trips with enum_from_name from enum-reflections.hpp.
            template <class E>
            requires json_enum<E>
            constexpr void write_enum(const E& value) {
                write_string(enum_names<E>[to_index(value)]);
            }

            //  A complete JSON string token: quotes added, contents escaped.
            constexpr void write_string(const std::string_view str) {
                *out_++ = '"';
                for (const char ch : str) {
                    write_escaped_char(ch);
                }
                *out_++ = '"';
            }

            constexpr void write_escaped_char(const char ch) {
                switch (ch) {
                    case '"': {
                        out_ = std::format_to(out_, "\\\"");
                        return;
                    }
                    case '\\': {
                        out_ = std::format_to(out_, "\\\\");
                        return;
                    }
                    case '\b': {
                        out_ = std::format_to(out_, "\\b");
                        return;
                    }
                    case '\f': {
                        out_ = std::format_to(out_, "\\f");
                        return;
                    }
                    case '\n': {
                        out_ = std::format_to(out_, "\\n");
                        return;
                    }
                    case '\r': {
                        out_ = std::format_to(out_, "\\r");
                        return;
                    }
                    case '\t': {
                        out_ = std::format_to(out_, "\\t");
                        return;
                    }
                    default: break;
                }
                if (const auto byte = static_cast<uint8_t>(ch); byte < 0x20) {
                    out_ = std::format_to(out_, "\\u{:04x}", byte);
                    return;
                }
                *out_++ = ch;
            }
    };

    //  Single-pass cursor over the JSON text: no backtracking, no token tree --
    //  parse_json<T>() dispatches on T and consumes exactly one value. Every
    //  failure throws std::runtime_error carrying the byte offset (fail() never
    //  returns). get_json<T>() is the only public entry point; it also rejects
    //  trailing characters after the value.
    class json_parser {
        public:
            explicit json_parser(const std::string_view src)
                : src_(src), pos_(0) {}

            //  Parse one complete value, then require the input to end.
            template <class T>
            T get_json() {
                T result = parse_json<T>();
                skip_ws();
                if (!at_end()) {
                    fail("trailing characters after value");
                }
                return result;
            }

        private:
            std::string_view src_;
            std::size_t pos_;

            [[noreturn]]
            void fail(const std::string& why) const {
                throw std::runtime_error(std::format("from_json: {} (offset {})", why, pos_));
            }

            [[nodiscard]]
            bool at_end() const {
                return pos_ >= src_.size();
            }

            [[nodiscard]]
            char peek() const {
                if (at_end()) {
                    fail("unexpected end of input");
                }
                return src_.at(pos_);
            }

            [[nodiscard]]
            char get() {
                const char c = peek();
                pos_++;
                return c;
            }

            void skip_ws() {
                while (!at_end() && (src_[pos_] == ' ' || src_[pos_] == '\t' || src_[pos_] == '\n' || src_[pos_] == '\r')) {
                    ++pos_;
                }
            }

            void expect(const char c) {
                skip_ws();
                if (at_end() || src_[pos_] != c) {
                    fail(std::format("expected '{}'", c));
                }
                pos_++;
            }

            bool consume(const char c) {
                skip_ws();
                if (at_end() || src_[pos_] != c) {
                    return false;
                }
                pos_++;
                return true;
            }

            bool consume_literal(const std::string_view lit) {
                skip_ws();
                if (src_.substr(pos_, lit.size()) != lit) {
                    return false;
                }
                pos_ += lit.size();
                return true;
            }

            [[nodiscard]]
            std::string parse_string() {
                expect('\"');
                std::string out;
                while (true) {
                    const char c = get();
                    if (c == '\"') {
                        //  This function's only exit point
                        return out;
                    }
                    if (c != '\\') {
                        out += c;
                        continue;
                    }

                    //  Escape sequence handling
                    switch (const char esc = get()) {
                        case '\"':
                        case '\\':
                        case '/': {
                            out += esc;
                            break;
                        }

                        case 'b': out += '\b'; break;
                        case 'f': out += '\f'; break;
                        case 'n': out += '\n'; break;
                        case 'r': out += '\r'; break;
                        case 't': out += '\t'; break;

                        //  Unicode escapes
                        case 'u': {
                            uint32_t cp = 0;
                            for (int i = 0; i < 4; i++) {
                                const char h = get();
                                cp <<= 4;
                                if (h >= '0' && h <= '9') {
                                    cp |= h - '0';
                                } else if (h >= 'A' && h <= 'F') {
                                    cp |= h - 'A' + 10;
                                } else if (h >= 'a' && h <= 'f') {
                                    cp |= h - 'a' + 10;
                                } else {
                                    fail("invalid hex digit in unicode escape");
                                }
                            }

                            if (cp < 0x80) {
                                //  ASCII Characters
                                out += static_cast<char>(cp);
                            }
                            else if (cp < 0x800) {
                                //  UTF-8 2-byte Characters
                                out += static_cast<char>(0xC0 | (cp >> 6));
                                out += static_cast<char>(0x80 | (cp & 0x3F));
                            }
                            else {
                                //  UTF-8 3-byte Characters
                                out += static_cast<char>(0xE0 | (cp >> 12));
                                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                                out += static_cast<char>(0x80 | (cp & 0x3F));
                            }

                            break;
                        }
                        default:
                            fail("invalid escape sequence");
                    }
                }
            }

            template <class N>
            N parse_number() {
                skip_ws();
                const auto first = src_.data() + pos_;
                const auto last = src_.data() + src_.size();

                N value{};
                const auto [ptr, ec] = std::from_chars(first, last, value);
                if (ec != std::errc{}) {
                    fail("invalid number");
                }
                pos_ += static_cast<std::size_t>(ptr - first);
                return value;
            }

            void skip_value() {
                skip_ws();

                switch (peek()) {
                    case '\"': {
                        (void) parse_string();
                        return;
                    }
                    case '{': {
                        (void) get();
                        if (consume('}')) return;
                        while (true) {
                            (void) parse_string();
                            expect(':');
                            skip_value();
                            if (consume(',')) continue;
                            expect('}');
                            return;
                        }
                    }
                    case '[': {
                        (void) get();
                        if (consume(']')) return;
                        while (true) {
                            skip_value();
                            if (consume(',')) continue;
                            break;
                        }
                        expect(']');
                        return;
                    }
                    default: {
                        if (consume_literal("true") || consume_literal("false") || consume_literal("null"))
                            return;

                        static constexpr std::string_view number_chars = "+-.eE0123456789";
                        const auto start = pos_;
                        for (; !at_end() && number_chars.contains(src_[pos_]); ++pos_){}
                        if (pos_ == start) {
                            fail("invalid value");
                        }
                    }
                }
            }

            template <class T>
            requires json_object<T>
            T parse_record() {
                static constexpr auto members = std::define_static_array(
                    std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())
                );

                T result{};
                expect('{');
                if (consume('}')) {
                    return result;
                }
                while (true) {
                    const auto key = parse_string();
                    expect(':');

                    bool matched = false;
                    template for (constexpr auto member: members) {
                        if (!matched && key == std::meta::identifier_of(member)) {
                            using M = [:std::meta::type_of(member):];
                            result.[:member:] = parse_json<M>();
                            matched = true;
                        }
                    }

                    if (!matched) {
                        skip_value();
                    }

                    if (consume(',')) {
                        continue;
                    }
                    break;
                }
                expect('}');
                return result;
            }

            template <class E>
            requires json_enum<E>
            E parse_enum() {
                return enum_from_name<E>(parse_string());
            }

            template <class T>
            T parse_json() {
                using U = std::remove_cvref_t<T>;

                if constexpr (std::is_same_v<U, char>) {
                    const auto s = parse_string();
                    if (s.size() != 1)
                        fail("expected a single-character string");
                    return s.front();
                }
                else if constexpr (std::is_same_v<U, bool>) {
                    if (consume_literal("true"))
                        return true;
                    if (consume_literal("false"))
                        return false;
                    fail("invalid bool");
                }
                else if constexpr (std::is_integral_v<U>) {
                    return parse_number<U>();
                }
                else if constexpr (std::is_floating_point_v<U>) {
                    if (consume_literal("null")) {
                        return std::numeric_limits<U>::quiet_NaN();
                    }
                    return parse_number<U>();
                }
                else if constexpr (std::is_same_v<U, std::string>) {
                    return parse_string();
                }
                else if constexpr (std::is_same_v<U, std::string_view>) {
                    static_assert(false, "from_json: std::string_view cannot own decoded text -- use std::string instead");
                }
                else if constexpr (json_enum<U>) {
                    return parse_enum<U>();
                }
                else if constexpr (json_object<U>) {
                    return parse_record<U>();
                }
                else if constexpr (std::ranges::input_range<U>) {
                    using Elem = std::ranges::range_value_t<U>;
                    U out{};
                    expect('[');
                    if (consume(']')) {
                        return out;
                    }
                    do {
                        out.push_back(parse_json<Elem>());
                    } while (consume(','));
                    expect(']');
                    return out;
                }
                else {
                    static_assert(false, "from_json: no JSON mapping for this type -- attach ANNOTATION(derive::json) to the struct or enum");
                }
                std::unreachable();
            }
    };
}

//  Serialize `value` into a JSON string. `indent` is the number of spaces
//  added per nesting level: 0 keeps the whole text on one line, n > 0
//  pretty-prints the way JSON.stringify(value, n) does. Supported today:
//  bool, all arithmetic types, char, std::string / std::string_view,
//  json-annotated enums and records, and any input range of supported
//  elements.
template <class T>
std::string to_json(const T& value, const std::size_t indent = 0) {
    std::string out;
    detail::json_format{std::back_inserter(out), indent}.write_json(value);
    return out;
}

//  Parse `value` back into T. Compact and pretty-printed text are accepted
//  alike (whitespace between tokens is skipped); characters trailing the
//  value are rejected. Malformed input throws std::runtime_error carrying
//  the offset of the offending token.
template <class T>
T from_json(const std::string_view value) {
    return detail::json_parser{value}.get_json<T>();
}
