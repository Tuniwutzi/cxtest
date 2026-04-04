#include <cxtest/cxtest.hpp>

#include <string-utils.hpp>

#include <thread>

namespace
{

namespace length
{
constexpr void success(cxt::Context& ctx)
{
    ctx.check(string_utils::strlen("") == 0);

    ctx.check(string_utils::strlen("abc") == 3);
    ctx.check(string_utils::strlen("abcd") == 4);
}

constexpr void error(cxt::Context& ctx)
{
    ctx.check_throws(
        []
        {
            string_utils::strlen(nullptr);
        });
}

} // namespace length

auto _ = cxt::register_tests<^^length>("strlen");

} // namespace
