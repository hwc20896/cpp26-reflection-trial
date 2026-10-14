# C++26 Reflection Trial

A small, self-contained reference for **C++26 static reflection** — [P2996](https://wg21.link/P2996) plus [P3394 annotations](https://wg21.link/P3394) — built up step by step into a working `derive(Debug)` in the spirit of Rust's `#[derive(Debug)]`.

No code is generated for any type: user types carry only annotations, and pre-written constrained formatters do the rest by reflecting over members at compile time.

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

This is a learning sandbox, not a library — expect the code to favor clarity over API stability.

## Toolchain

Experimental territory. Pin your versions; "latest GCC" will not do:

| Component | Version used and verified here |
|---|---|
| OS | Windows 11 |
| Compiler | GCC **16.1.0** with reflection support — `c++.exe (Rev5, Built by MSYS2 project)`, target `x86_64-w64-mingw32`, via MSYS2 UCRT64 |
| Library | libstdc++ **exp** (`libstdc++exp`) — supplies the terminal I/O helpers for `std::print` / `std::println`; `std::meta` itself needs no extra library |
| Language flags | `-std=c++26 -freflection` |
| CMake | ≥ 3.25 (first release where `26` is a valid `CXX_STANDARD`; optional — a single `g++` command also works) |

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

`main.cpp` defines three annotated types and prints them with plain `std::println("{}", ...)`:

```
Foo { b: 'a', c: true, message: "hello but\n this ain't gonna work\\What if I beep\u{7}" }
Test::A
Bar { f: Foo { b: 'a', c: true, message: "hello but\n this ain't gonna work\\What if I beep\u{7}" }, v: [2, 3, 4, 5, 6, 7, 8, 9, 10] }
```

Three things to notice:

- `a` is gone (`debug_skip` works), `d` became `message` (`debug_rename` works), and the string is escaped exactly like Rust's `Debug`: quote-sensitive, non-printable control characters as `\u{XX}`.
- The enum prints its fully qualified name, `Test::A`.
- Recursion and ranges are free: `Bar.f` reuses `Foo`'s formatter, and `v` goes through the standard range formatter.

## The pattern

End to end, `derive(Debug)` is three steps:

1. **The switch** — a marker annotation attached to a type with `[[=...]]`. `ANNOTATION(x)` is just shorthand for `[[=x]]`, so attachment sites read `ANNOTATION(derive::debug)`.
2. **The detection** — a concept that reads the annotation back with `std::meta::annotations_of_with_type` (here: `derive::debug_derived<T>`).
3. **The implementation** — a constrained `std::formatter<T>` partial specialization that reflects over the type: `nonstatic_data_members_of` + `template for` + member splice `t.[:member:]`, dispatching per member.

There is no injection and no per-type code generation — just generic code selected by annotations.

## Repository tour

Suggested reading order; every file is one layer:

1. **[`src/annotations.hpp`](src/annotations.hpp)** — annotation basics: how to attach compile-time values to declarations, how to read them back, the `ANNOTATION` macro, and a string-annotation example (`StrAnnotation`).
2. **[`src/reflection-util.hpp`](src/reflection-util.hpp)** — the mechanism layer: concepts (`is_enum`, `nonenum_class`, `has_members`), `member_size`, and the two walkers (`metadata_array`, `metadata_multiple_array`) that project member reflections into compile-time arrays.
3. **[`src/enum-reflections.hpp`](src/enum-reflections.hpp)** — the enum facade: `enum_size`, `enum_values`, `enum_names`, `enum_full_names`, `enum_name_value_pairs`, plus lookups `to_index` / `enum_from_name`.
4. **[`src/debug-formatter.hpp`](src/debug-formatter.hpp)** — the application: the `DEFINE_ANNO_*` macros, Rust-compatible escape helpers, and the two constrained formatters that implement `derive(Debug)`.
5. **[`main.cpp`](main.cpp)** — the demo; only annotations, no machinery.

## Feature matrix

| Feature | Rust | This repo |
|---|---|---|
| enable per type | `#[derive(Debug)]` | `ANNOTATION(derive::debug)` |
| struct format | `Foo { a: 1, ... }` | same |
| enum format | `Test::A` | same |
| string escaping | `escape_debug` | `\"` `\\` `\n` `\r` `\t` `\0` `\u{XX}` |
| skip a field | via crates (e.g. serde) | `ANNOTATION(derive::debug_skip)` |
| rename a field | via crates (e.g. serde) | `ANNOTATION(derive::debug_rename("message"))` |

## Known limitations

- **Access control**: the record formatter reflects with `std::meta::access_context::current()`, so it only sees members accessible at the point of instantiation. Types with private fields need the instantiation to happen where access exists.
- **No format spec**: `{}` only — `{:#?}`-style flags, widths, and alignment are not implemented.
- **Wild enum values**: `to_index()` throws `std::out_of_range` for non-sequential enums, but takes an unchecked fast path for sequential zero-based enums.
- **String payloads in annotations** must live in static storage; see `StrAnnotation` in `annotations.hpp` for the `std::define_static_string` pattern that makes this work.

## References

- [P2996 — Reflection for C++26](https://wg21.link/P2996)
- [P3394 — Annotations for Reflection](https://wg21.link/P3394)
