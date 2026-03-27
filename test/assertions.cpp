#include <jtest/jtest.hpp>

#include "check.hpp"

namespace
{

namespace tests
{

constexpr void check_rt(auto& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}

constexpr void check_ct(auto& ctx)
{
    ctx.check(std::is_constant_evaluated());
}

constexpr void check_multiple_failures(auto& ctx)
{
    ctx.check(false);
    ctx.check(false);
    ctx.check(false);
}

constexpr void check_then_require(auto& ctx)
{
    ctx.check(false);
    ctx.require(false);
    ctx.check(false);
}

} // namespace tests

} // namespace

void test_assertions()
{
    jtest::TestGroup<^^tests> group{jtest::skip_registration};
    auto results = jtest::run_group(group);

    CHECK(results.test_results.size() == 4, "Expected 4 tests to run");

    {
        auto& [ct, rt] = results.test_results.at("check_rt");
        CHECK(rt.value().errors.empty() && !ct.value().errors.empty(),
              "check_rt should succeed at runtime and fail at compiletime");
    }

    {
        auto& [ct, rt] = results.test_results.at("check_ct");
        CHECK(!rt.value().errors.empty() && ct.value().errors.empty(),
              "check_rt should succeed at runtime and fail at compiletime");
    }

    {
        auto& [ct, rt] = results.test_results.at("check_multiple_failures");
        auto check = [](auto& result)
        {
            CHECK(result.value().errors.size() == 3, "check_multiple_failures expects 3 failures");
        };
        check(ct);
        check(rt);
    }

    {
        auto& [ct, rt] = results.test_results.at("check_then_require");
        auto check = [](auto& result)
        {
            CHECK(result.value().errors.size() == 2, "check_then_require expects 2 failures");
        };
        check(ct);
        check(rt);
    }
}