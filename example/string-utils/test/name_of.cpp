#include <cxtest/cxtest.hpp>
#include <string-utils.hpp>

namespace
{

struct Foo
{
};

namespace name_of
{

// Compiletime only
consteval void test_meta_name_of(cxtest::CTContext& ctx)
{
    ctx.check(string_utils::name_of(^^int) == "int");
    ctx.check(string_utils::name_of(^^name_of) == "name_of");
}

// Compile- and Runtime
constexpr void test_templated_name_of(cxtest::Context& ctx)
{
    ctx.check(string_utils::name_of<int>() == "int");
    ctx.check(string_utils::name_of<Foo>() == "Foo");
}

// Runtime only
void test_rtti_name_of(cxtest::RTContext& ctx)
{
    ctx.check(string_utils::name_of(typeid(int)) == typeid(int).name());
    ctx.check(string_utils::name_of(typeid(Foo)) == typeid(Foo).name());
}

} // namespace name_of

const auto _ = cxtest::register_tests_in_namespace_recursive<^^name_of>();

} // namespace