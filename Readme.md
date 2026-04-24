cxtest (short for constexpr test) is a unit test framework based on C++26, that treats compiletime and runtime testing as equal.

Core ideas:
- Allow writing tests that can be executed both at runtime and compiletime, with the same syntax and techniques familiar from other testing frameworks.
- Make assertion failures in compiletime test execution _not_ fail compliation. Instead we embed the failures into the binary and handle them the same way as assertion failures in runtime tests: error messages are printed when running the test executable.

cxtest uses the [MIT license](./LICENSE).

# Basic usage

```c++
// cxtest header
#include <cxtest/cxtest.hpp>

// Example library we want to test
#include <string-utils/string-utils.hpp>

namespace string_utils_tests {

// Define a test to run at both compile- and runtime
constexpr void test_strlen(cxtest::Context& ctx) {
    ctx.check(string_utils::strlen("abc") == 3);
}

}

// In practice: use `_` as a placeholder name, which is a C++26 feature.
// Here we call it `registration` to clearly show that a global variable is created.
static const auto registration = cxtest::register_tests_in_namespace_recursive<^^string_utils_tests>();
```

## Execution Model

The context parameter is used to distinguish whether a test is intended to run at runtime (`cxtest::RTContext`), compiletime (`cxtest::CTContext`) or both (`cxtest::Context`). In this example, `test_strlen` will be executed both at compiletime and at runtime.

The compiletime run of a test is done as soon as the test is discovered. Failures are embedded into the binary and do not lead to compilation failure. When running the test executable, failures that occurred during compiletime testing will be reported alongside potential runtime errors.

### Caveat

In a perfect world the test declarations would look like this:
```c++
// !!!!!!!!
// This is fictional! Not how cxtest actually works!
constexpr void test1(cxtest::Context&); // CT and RT
consteval void test2(cxtest::Context&); // CT only
          void test3(cxtest::Context&); // RT only
// !!!!!!!!
```

But C++26 reflection does not allow us to check for `constexpr`-/`consteval`-ness of a function. So instead we choose to rely on parameter types (`Context`, `CTContext`, `RTContext`) to carry that information. This means the test author must ensure the test is declared with the correct combination of context type and `constexpr`/`consteval`. The rules are:
- Tests taking `Context` must be `constexpr`
- Tests taking `CTContext` must be `constexpr` or `consteval`
- Tests taking `RTContext` must not be `consteval`

## Discovery

The registration discovers all tests in the given namespace(s) and registers them so they can be executed across TU boundaries. Namespaces are used to create named groups of tests. So in the example, we'll have one group named "string_utils_tests" with a single test named "test_strlen".

A test function is discovered when it fulfills these criteria:
- Is a function (`std::meta::is_function`).
- Is invocable with a single argument of type `CTContext&` or `RTContext&`.

A namespace becomes a test group if it contains at least one test. Nested namespaces don't become subgroups, they function independently, but their name will be the fully qualified namespace. Example:
```c++
namespace foo { namespace bar {
void baz(cxtest::RTContext&);
}}
```
This will yield _one_ group named "foo::bar" with a test "baz". If we add a test function directly into `foo` as well, we get two groups: "foo" with the new test, and "foo::bar" with test "baz".

## Entrypoint and Output

You can either write your own `main` function to execute the tests via `cxtest::run_registered_tests`, or link the CMake target `cxtest_main`, which defines a suitable cxtest entrypoint for you.

The output of the example above looks like this:
```
Executing group string_utils_tests with 1 tests
        CT execution of test_strlen... succeeded
        RT execution of test_strlen... succeeded
```

And if we change the test to fail only at runtime:
```
Executing group string_utils_tests with 1 tests
        CT execution of test_strlen... succeeded
        RT execution of test_strlen... failed
                Check failed at C:\dev\tmp\tmp.cpp:18
```

For more comprehensive examples see [the `example` directory](./example).

# Getting started

CMake is the only explicitly supported way to use cxtest. Once you have the cxtest source available, simply use `add_subdirectory` and link to `cxtest` or `cxtest_main`. cxtest will skip compiling tests of cxtest itself if it is included as a subdirectory.

## Minimal setup

CMakeLists.txt:
```cmake
cmake_minimum_required(VERSION 3.30.0)
project(my_test_project LANGUAGES CXX)

add_subdirectory("path/to/cxtest")
add_executable(my_test_project test.cpp)
target_link_libraries(my_test_project PRIVATE cxtest_main)
```

test.cpp:
```c++
#include <cxtest/cxtest.hpp>

namespace tests {
constexpr void test_example(cxtest::Context& ctx) {
    ctx.check(true);
}
}

static const auto _ = cxtest::register_tests_in_namespace_recursive<^^tests>();
```

# Requirements

cxtest relies on a lot of cutting edge C++26 features, most notably:
- Deep support of C++26 reflection ([P2996](https://isocpp.org/files/papers/P2996R13.html), [P3096](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3096r12.pdf), [P3491](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3491r3.html))
- Compile time virtual inheritance ([P3533](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3533r2.html))
- Compile time exception handling ([P3068](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p3068r6.html))

At the time of writing, the only compiler capable of this is GCC 16.1 with the `-freflection` flag.

# Stability

cxtest is very much experimental and in development. APIs may change greatly. Until we have a 1.0 release, assume breaking changes can occur in every release.

# API

The main API surface is `cxtest::Context`, which contains all functionality used within tests. The general vocabulary is: `check` = the test continues executing after a failure, `require` = throws a unique exception type (not inheriting `std::exception`) to exit the current test.

Currently supported:
- `[check|require](bool, std::string msg = <default msg>)`: fails the test with the given msg if the bool is false.
- `[check|require]_throws(auto functor, std::string msg = <default msg>)`: fails the test with the given msg if the functor does not throw.
- `[check|require]_nothrow(auto functor, std::string msg = <default msg>)`: fails the test with the given msg if the functor throws.

The `<default msg>` contains the source location of the failing assertion within the test.

## Missing features

- Fixtures (setup/teardown)
- Better failure output (e.g.: `check_eq(a, b)` that prints the content of `a`  and `b` upon failure)
- Filtering tests
