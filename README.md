# C++26 Reflection Trial

A small, self-contained reference for **C++26 static reflection** — [P2996](https://wg21.link/P2996) plus [P3394 annotations](https://wg21.link/P3394) — built up step by step into a working `derive(Debug)` and a serde-style `derive(JSON)`, in the spirit of Rust's `#[derive(Debug)]` / `#[derive(Serialize, Deserialize)]`.

No code is generated for any type: user types carry only annotations, and pre-written constrained formatters and function templates do the rest by reflecting over members at compile time.

```cpp
enum class ANNOTATION(derive::debug) Test { A = 3, B = 8, C = 16, D = 29 };

struct ANNOTATION(derive::debug) Foo {
    int a ANNOTATION(derive::debug_skip);
    char b;
    bool c;
    std::string d ANNOTATION(derive::debug_rename("message"));
};

std::println("{}", Foo{1, 'a', true, "hello"});
//  Foo { b: 'a', c: true, message: "hello" }
```

```cpp
struct ANNOTATION(derive::json) Snack { std::string name; float price; int quantity; };

std::println("{}", to_json(Snack{"chips", 0.99, 30}));
//  {"name":"chips","price":0.99,"quantity":30}
```

This is a learning sandbox, not a library — expect the code to favor clarity over API stability.

## Toolchain

Experimental territory. Pin your versions; "latest GCC" will not do:

| Component      | Version used and verified here                                                                                                                          |
|----------------|---------------------------------------------------------------------------------------------------------------------------------------------------------|
| OS             | Windows 11                                                                                                                                              |
| Compiler       | GCC **16.2.0** with reflection support — `c++.exe (Rev3, Built by MSYS2 project)`, target `x86_64-w64-mingw32`, via MSYS2 UCRT64 (16.1.0 also verified) |
| Library        | libstdc++ **exp** (`libstdc++exp`) — supplies the terminal I/O helpers for `std::print` / `std::println`; `std::meta` itself needs no extra library     |
| Language flags | `-std=c++26 -freflection`                                                                                                                               |
| CMake          | ≥ 3.25 (first release where `26` is a valid `CXX_STANDARD`; optional — a single `g++` command also works)                                               |

> [!WARNING] 
> C++26 reflection is not final. The syntax exercised here follows GCC's current experimental implementation and may drift or break in future releases. If you build on this, pin the compiler.

## Build & run

With CMake:

```sh
cmake -S . -B build
cmake --build build
./build/cpp26_reflection_trial      # build\cpp26_reflection_trial.exe on Windows
```

Or manually, with no build system at all:

```sh
g++ -std=c++26 -freflection -Isrc main.cpp -lstdc++exp -o trial
```

The two things that matter: `-freflection` enables the reflection and annotation language features, and `stdc++exp` supplies the terminal I/O helpers that `std::print` / `std::println` use. The reflection machinery itself (`^^`, splices, `std::meta`) comes from the compiler alone.

> **Note**: without `-lstdc++exp`, linking a program that calls `std::println` fails with `undefined reference to 'std::__open_terminal(_iobuf*)'` and `'std::__write_to_terminal(...)'` — verified with the pinned toolchain above, and this repo's demo uses `std::println`, so the link is required here. If you would rather not link it, `std::cout << std::format(...)` links clean with no extra library.

## Demo

`main.cpp` defines five annotated types. The three Debug-annotated ones (`Test`, `Foo`, `Bar`) print with plain `std::println("{}", ...)`:

```
Foo { b: 'a', c: true, message: "hello but\n this ain't gonna work\\What if I beep\u{7}" }
Test::A
Bar { f: Foo { b: 'a', c: true, message: "hello but\n this ain't gonna work\\What if I beep\u{7}" }, v: [2, 3, 4, 5, 6, 7, 8, 9, 10] }
```

Three things to notice:

- `a` is gone (`debug_skip` works), `d` became `message` (`debug_rename` works), and the string is escaped exactly like Rust's `Debug`: quote-sensitive, non-printable control characters as `\u{XX}`.
- The enum prints its fully qualified name, `Test::A`.
- Recursion and ranges are free: `Bar.f` reuses `Foo`'s formatter, and `v` goes through the standard range formatter.

`Foo`, `Snack`, and `Stock` additionally carry `ANNOTATION(derive::json)`; their stringify / parse round-trips are shown in the [JSON](#json) section.

## The pattern

End to end, `derive(Debug)` is three steps:

1. **The switch** — a marker annotation attached to a type with `[[=...]]`. `ANNOTATION(x)` is just shorthand for `[[=x]]`, so attachment sites read `ANNOTATION(derive::debug)`.
2. **The detection** — a concept that reads the annotation back with `std::meta::annotations_of_with_type` (here: `derive::debug_derived<T>`).
3. **The implementation** — a constrained `std::formatter<T>` partial specialization that reflects over the type: `nonstatic_data_members_of` + `template for` + member splice `t.[:member:]`, dispatching per member.

There is no injection and no per-type code generation — just generic code selected by annotations.

## JSON

The same annotation idea drives a serde-style `derive(JSON)`: attach `ANNOTATION(derive::json)` and the generic `to_json` / `from_json` function templates do the rest. Debug-only annotations (`debug_skip`, `debug_rename`) deliberately do not apply here — JSON mirrors the members exactly as declared.

| Rust / serde                        | This repo                  |
|-------------------------------------|----------------------------|
| `#[derive(Serialize, Deserialize)]` | `ANNOTATION(derive::json)` |
| `serde_json::to_string`             | `to_json(v)`               |
| `serde_json::to_string_pretty`      | `to_json(v, indent)`       |
| `serde_json::from_str`              | `from_json<T>(s)`          |

Semantics follow `JSON.stringify` where they overlap:

- **Minimal escaping** (RFC 8259 / RFC 8785): only `"`, `\`, and control characters are escaped — controls as `\u00XX`.
- **inf / nan write as `null`** (JSON has no such numbers); on read, `null` into a floating-point member yields NaN.
- **Enums** write their bare enumerator name (`"A"`), parsed back through `enum_from_name`.
- `indent` is the number of spaces added per nesting level; the default `0` keeps everything on one line.
- On read: **unknown keys are skipped**, **missing keys keep the `T{}` default**, compact and pretty text parse alike, and trailing characters after the value are rejected. Malformed input throws `std::runtime_error` with the byte offset of the offending token.

Nesting is free, and the pretty output lines up with `JSON.stringify(stock, 2)` column for column:

```
{
  "open": true,
  "snacks": [
    {
      "name": "chips",
      "price": 0.99,
      "quantity": 30
    },
    ...
  ]
}
```

## Repository tour

Suggested reading order; every file is one layer:

1. **[`src/annotations.hpp`](src/annotations.hpp)** — annotation basics: how to attach compile-time values to declarations, how to read them back, the `ANNOTATION` macro, and a string-annotation example (`StrAnnotation`).
2. **[`src/reflection-util.hpp`](src/reflection-util.hpp)** — the mechanism layer: concepts (`is_enum`, `nonenum_class`, `has_members`), `member_size`, and the two walkers (`metadata_array`, `metadata_multiple_array`) that project member reflections into compile-time arrays.
3. **[`src/enum-reflections.hpp`](src/enum-reflections.hpp)** — the enum facade: `enum_size`, `enum_values`, `enum_names`, `enum_full_names`, `enum_name_value_pairs`, plus lookups `to_index` / `enum_from_name`.
4. **[`src/debug-formatter.hpp`](src/debug-formatter.hpp)** — the application: the `DEFINE_ANNO_*` macros, Rust-compatible escape helpers, and the two constrained formatters that implement `derive(Debug)`.
5. **[`src/json.hpp`](src/json.hpp)** — the serde-style layer: the write / read dispatch chains, RFC 8259 string escaping, the `json_format` pretty-printer state, and the public `to_json` / `from_json`.
6. **[`main.cpp`](main.cpp)** — the demo; only annotations, no machinery.

## Feature matrix

| Feature         | Rust                    | This repo                                     |
|-----------------|-------------------------|-----------------------------------------------|
| enable per type | `#[derive(Debug)]`      | `ANNOTATION(derive::debug)`                   |
| struct format   | `Foo { a: 1, ... }`     | same                                          |
| enum format     | `Test::A`               | same                                          |
| string escaping | `escape_debug`          | `\"` `\\` `\n` `\r` `\t` `\0` `\u{XX}`        |
| skip a field    | via crates (e.g. serde) | `ANNOTATION(derive::debug_skip)`              |
| rename a field  | via crates (e.g. serde) | `ANNOTATION(derive::debug_rename("message"))` |

## Known limitations

- **Access control**: the Debug formatter and the JSON chains reflect with `std::meta::access_context::current()`, so they only see members accessible at the point of instantiation. Types with private fields need the instantiation to happen where access exists.
- **No format spec**: `{}` only — `{:#?}`-style flags, widths, and alignment are not implemented.
- **JSON `\uXXXX` escapes** are not merged into UTF-16 surrogate pairs — an escaped astral character decodes to two halves of invalid UTF-8. Raw UTF-8 input passes through byte-for-byte and round-trips fine.
- **JSON reads need owning types**: `std::string_view` members are rejected at compile time (use `std::string`), ranges need `push_back` (`std::vector` yes, `std::array` no), and objects decode into annotated records only — no map-like containers.
- **String payloads in annotations** must live in static storage; see `StrAnnotation` in `annotations.hpp` for the `std::define_static_string` pattern that makes this work.

## References

- [P2996 — Reflection for C++26](https://wg21.link/P2996)
- [P3394 — Annotations for Reflection](https://wg21.link/P3394)
