//  Demo: user code only attaches annotations -- no Debug machinery is
//  written by hand here. The formatters and the reflection live in
//  src/debug-formatter.hpp.

#include "src/debug-formatter.hpp"
#include <print>

enum class ANNOTATION(derive::debug) Test {
    //  Just a bunch of random numbers.
    A = 3,
    B = 8,
    C = 16,
    D = 29
};

struct ANNOTATION(derive::debug) Foo {
    int a ANNOTATION(derive::debug_skip);
    char b;
    bool c;
    std::string d ANNOTATION(derive::debug_rename("message"));
};

struct ANNOTATION(derive::debug) Bar {
    Foo f;
    std::vector<int> v;
};

int main() {
    Foo f{1, 'a', true, "hello but\n this ain't gonna work\\What if I beep\a"};
    std::println("{}", f);

    Test t = Test::A;
    std::println("{}", t);

    Bar b{.f=f, .v={2,3,4,5,6,7,8,9,10}};
    std::println("{}", b);
}