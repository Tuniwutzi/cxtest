#include <cxtest/cxtest.hpp>

#include <string-utils.hpp>

#include <thread>

namespace
{

namespace length
{
constexpr void success(cxtest::Context& ctx)
{
    ctx.check(string_utils::strlen("") == 0);

    ctx.check(string_utils::strlen("abc") == 3);
    ctx.check(string_utils::strlen("abcd") == 4);
}

constexpr void error(cxtest::Context& ctx)
{
    ctx.check_throws(
        []
        {
            string_utils::strlen(nullptr);
        });
}

} // namespace length

auto _ = cxtest::register_tests_in_namespace_recursive<^^length>();

} // namespace
