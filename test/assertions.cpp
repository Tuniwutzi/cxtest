#include <cxtest/cxtest.hpp>

#include "check.hpp"

namespace
{

namespace tests
{

constexpr void check_rt(cxtest::Context& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}

constexpr void check_ct(cxtest::Context& ctx)
{
    ctx.check(std::is_constant_evaluated());
}

constexpr void check_multiple_failures(cxtest::Context& ctx)
{
    ctx.check(false);
    ctx.check(false);
    ctx.check(false);
}

constexpr void check_then_require(cxtest::Context& ctx)
{
    ctx.check(false);
    ctx.require(false);
    ctx.check(false);
}

constexpr void check_nothrow(cxtest::Context& ctx)
{
    ctx.check_nothrow([] {}, "Success");
    ctx.check_nothrow(
        []
        {
            throw 1;
        },
        "Failure");
    ctx.require_nothrow([] {}, "ReqSuccess");
    ctx.require_nothrow(
        []
        {
            throw 1;
        },
        "ReqFailure");
    ctx.check(false, "Unreachable");
}

constexpr void check_throws(cxtest::Context& ctx)
{
    ctx.check_throws(
        []
        {
            throw 1;
        },
        "Success");
    ctx.check_throws([] {}, "Failure");

    ctx.require_throws(
        []
        {
            throw 1;
        },
        "ReqSuccess");
    ctx.require_throws([] {}, "ReqFailure");

    ctx.check(false, "Unreachable");
}

} // namespace tests

} // namespace

void test_assertions()
{
    auto group = cxtest::group_tests<^^tests>();
    cxtest::CollectingGroupOutputSink sink{};
    cxtest::run_group(group, sink);

    REQUIRE(sink.tests.size() == 6, "Expected 6 tests to run");

    {
        auto& [ct, rt] = sink.tests.at("check_rt");
        REQUIRE(rt.value().errors.empty() && !ct.value().errors.empty(),
                "check_rt should succeed at runtime and fail at compiletime");
    }

    {
        auto& [ct, rt] = sink.tests.at("check_ct");
        REQUIRE(!rt.value().errors.empty() && ct.value().errors.empty(),
                "check_rt should succeed at runtime and fail at compiletime");
    }

    {
        auto& [ct, rt] = sink.tests.at("check_multiple_failures");
        auto check = [](auto& result)
        {
            REQUIRE(result.value().errors.size() == 3, "check_multiple_failures expects 3 failures");
        };
        check(ct);
        check(rt);
    }

    {
        auto& [ct, rt] = sink.tests.at("check_then_require");
        auto check = [](auto& result)
        {
            REQUIRE(result.value().errors.size() == 2, "check_then_require expects 2 failures");
        };
        check(ct);
        check(rt);
    }

    {
        auto& [ct, rt] = sink.tests.at("check_nothrow");
        auto check = [](auto& result)
        {
            auto equal = std::ranges::equal(result.value().errors, std::vector<std::string>{"Failure", "ReqFailure"});
            REQUIRE(equal, "check_nothrow expects 2 specific failures");
        };
        check(ct);
        check(rt);
    }

    {
        auto& [ct, rt] = sink.tests.at("check_throws");
        auto check = [](auto& result)
        {
            auto equal = std::ranges::equal(result.value().errors, std::vector<std::string>{"Failure", "ReqFailure"});
            REQUIRE(equal, "check_throws expects 2 specific failures");
        };
        check(ct);
        check(rt);
    }
}