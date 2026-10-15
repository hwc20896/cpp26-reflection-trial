//  Demo: user code only attaches annotations -- no Debug or JSON machinery
//  is written by hand here. The Debug formatters and the reflection live in
//  src/debug-formatter.hpp; to_json / from_json live in src/json.hpp.

#include "src/debug-formatter.hpp"
#include "src/json.hpp"
#include <print>

enum class ANNOTATION(derive::debug) Test {
    //  Just a bunch of random numbers.
    A = 3,
    B = 8,
    C = 16,
    D = 29
};

struct ANNOTATION(derive::debug) ANNOTATION(derive::json) Foo {
    int a ANNOTATION(derive::debug_skip);
    char b;
    bool c;
    std::string d ANNOTATION(derive::debug_rename("message"));
};

struct ANNOTATION(derive::debug) Bar {
    Foo f;
    std::vector<int> v;
};

struct ANNOTATION(derive::json) ANNOTATION(derive::debug) Snack {
    std::string name;
    float price;
    int quantity;
};

struct ANNOTATION(derive::json) Stock {
    bool open;
    std::vector<Snack> snacks;
};

int main() {
    Foo f{1, 'a', true, "hello but\n this ain't gonna work\\What if I beep\a"};
    std::println("{}", f);

    Test t = Test::A;
    std::println("{}", t);

    Bar b{.f=f, .v={2,3,4,5,6,7,8,9,10}};
    std::println("{}", b);

    std::string json;
    std::println("JSON.stringify(f) = {}", json = to_json(f, 2));
    std::println("JSON.parse(JSON.stringify(f)) = {}", from_json<Foo>(json));

    std::string snack = R"json({"name":"chips","price":0.99,"quantity":30})json";
    std::println("JSON.parse(snack) = {}", from_json<Snack>(snack));

    const auto stock = Stock{.open=true, .snacks={
        Snack{"chips", 0.99, 30},
        Snack{"popcorn", 1.99, 20},
        Snack{"candy", 0.50, 40}
    }};

    std::println("JSON.stringify(stock) = {}", to_json(stock, 2));
    // JSON.stringify(stock) = {
    //   "open": true,
    //   "snacks": [
    //     {
    //       "name": "chips",
    //       "price": 0.99,
    //       "quantity": 30
    //     },
    //     {
    //       "name": "popcorn",
    //       "price": 1.99,
    //       "quantity": 20
    //     },
    //     {
    //       "name": "candy",
    //       "price": 0.5,
    //       "quantity": 40
    //     }
    //   ]
    // }
}