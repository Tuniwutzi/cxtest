#include <jtest/jtest.hpp>

#include <string-utils.hpp>

#include <thread>

namespace
{

namespace length
{
constexpr void success(jtest::Context& ctx)
{
    ctx.check(string_utils::strlen("") == 0);

    ctx.check(string_utils::strlen("abc") == 3);
    ctx.check(string_utils::strlen("abcd") == 4);
}

constexpr void error(jtest::Context& ctx)
{
    ctx.check_throws(
        []
        {
            string_utils::strlen(nullptr);
        });
}

} // namespace length

auto _ = jtest::register_tests<^^length>("strlen");

} // namespace
