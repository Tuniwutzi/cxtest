#include <cxtest/cxtest.hpp>

#include "check.hpp"

#include <algorithm>
#include <format>
#include <source_location>

namespace
{

namespace[[= cxtest::group()]] invalid
{
// These tests should fail compilation in ways we can't catch and diagnose

// Ask to execute at compiletime without constexpr or consteval
// void comptime(cxtest::CTContext& ctx) {}
// void comptime(cxtest::Context& ctx) {}

// Ask to execute at runtime with consteval
// consteval void runtime(cxtest::RTContext& ctx) {}
// consteval void runtime(cxtest::Context& ctx) {}

} // namespace invalid

void test_invalid()
{
    bool threw = [] consteval
    {
        try
        {
            cxtest::detail::discover_group_namespace(^^invalid);
            return false;
        }
        catch (const std::meta::exception&)
        {
            return true;
        }
    }();
    REQUIRE(threw, "Invalid group should throw");
}

namespace[[= cxtest::group()]] valid
{
// Constexpr can request anything:
constexpr void cx_both_1(cxtest::Context& ctx) {}
constexpr void cx_ct_1(cxtest::CTContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void cx_rt_1(cxtest::RTContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}

// Consteval can request only compiletime execution:
consteval void ce_1(cxtest::CTContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}

// Without consteval or constexpr we can only request runtime execution:
void rt_1(cxtest::RTContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
} // namespace valid

void test_valid()
{
    auto valid_group = discover_and_instantiate_group<^^valid>();
    cxtest::CollectingGroupOutputSink sink{};
    valid_group.run(sink);
    REQUIRE(sink.tests.size() == 5, "Unexpected number of tests ran");
    for (const auto& [name, result] : sink.tests)
    {
        const auto& [ct, rt] = result;
        if (name.starts_with("cx_both"))
        {
            REQUIRE(ct && rt, "cx_both should run compiletime and runtime tests");
        }
        else if (name.starts_with("cx_ct"))
        {
            REQUIRE(ct && !rt, "cx_ct should run only compiletime tests");
        }
        else if (name.starts_with("cx_rt"))
        {
            REQUIRE(!ct && rt, "cx_rt should run only runtime tests");
        }
        else if (name.starts_with("ce"))
        {
            REQUIRE(ct && !rt, "ce should run only compiletime tests");
        }
        else if (name.starts_with("rt"))
        {
            REQUIRE(!ct && rt, "rt should run only runtime tests");
        }
        else
        {
            REQUIRE(false, std::format("Unknown test name for {}", name));
        }

        if (ct)
        {
            REQUIRE(ct->failures.empty(), std::format("Compiletime tests should be successful for {}", name));
        }
        if (rt)
        {
            REQUIRE(rt->failures.empty(), std::format("Runtime tests should be successful for {}", name));
        }
    }
}

} // namespace

void test_execution()
{
    test_invalid();
    test_valid();
}
