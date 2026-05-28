#include <cxtest/cxtest.hpp>

#include "check.hpp"

namespace
{

namespace[[= cxtest::test_group()]] tests
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

constexpr void check_uncaught(cxtest::Context& ctx)
{
    ctx.check(false, "Failure 1");
    throw std::runtime_error{"unique string included in this uncaught exception"};
    ctx.check(false, "Failure 2");
}

constexpr void check_uncaught_unknown_type(cxtest::Context& ctx)
{
    ctx.check(false, "Failure 1");
    struct Foo
    {
    };
    throw Foo{};
    ctx.check(false, "Failure 2");
}

} // namespace tests

} // namespace

void test_assertions()
{
    auto group = discover_and_instantiate_group<^^tests>();
    cxtest::CollectingGroupOutputSink sink{};
    group.run(sink);

    REQUIRE(sink.tests.size() == 8, "Unexpected number of tests ran");

    {
        auto& [ct, rt] = sink.tests.at("check_rt");
        REQUIRE(rt.value().failures.empty() && !ct.value().failures.empty(),
                "check_rt should succeed at runtime and fail at compiletime");
    }

    {
        auto& [ct, rt] = sink.tests.at("check_ct");
        REQUIRE(!rt.value().failures.empty() && ct.value().failures.empty(),
                "check_rt should succeed at runtime and fail at compiletime");
    }

    {
        auto& [ct, rt] = sink.tests.at("check_multiple_failures");
        auto check = [](auto& result)
        {
            REQUIRE(result.value().failures.size() == 3, "check_multiple_failures expects 3 failures");
        };
        check(ct);
        check(rt);
    }

    {
        auto& [ct, rt] = sink.tests.at("check_then_require");
        auto check = [](auto& result)
        {
            REQUIRE(result.value().failures.size() == 2, "check_then_require expects 2 failures");
        };
        check(ct);
        check(rt);
    }

    {
        auto& [ct, rt] = sink.tests.at("check_nothrow");
        auto check = [](auto& result)
        {
            auto equal = std::ranges::equal(result.value().failures, std::vector<std::string>{"Failure", "ReqFailure"});
            REQUIRE(equal, "check_nothrow expects 2 specific failures");
        };
        check(ct);
        check(rt);
    }

    {
        auto& [ct, rt] = sink.tests.at("check_throws");
        auto check = [](auto& result)
        {
            auto equal = std::ranges::equal(result.value().failures, std::vector<std::string>{"Failure", "ReqFailure"});
            REQUIRE(equal, "check_throws expects 2 specific failures");
        };
        check(ct);
        check(rt);
    }

    {
        auto& [ct, rt] = sink.tests.at("check_uncaught");
        auto check = [](auto& result)
        {
            auto& failures = result.value().failures;
            REQUIRE(failures.size() == 2, "Expected exactly 2 failures");
            REQUIRE(failures.front() == "Failure 1", "Unexpected first failure");
            REQUIRE(failures.back().contains("unique string included in this uncaught exception"),
                    "Exception message not included in failure");
        };
        check(ct);
        check(rt);
    }
    {
        auto& [ct, rt] = sink.tests.at("check_uncaught_unknown_type");
        auto check = [](auto& result)
        {
            auto& failures = result.value().failures;
            REQUIRE(failures.size() == 2, "Expected exactly 2 failures");
            REQUIRE(failures.front() == "Failure 1", "Unexpected first failure");
            REQUIRE(failures.back() == "Exception of unknown type escaped the test function",
                    "Unexpected message for uncaught exception");
        };
        check(ct);
        check(rt);
    }
}