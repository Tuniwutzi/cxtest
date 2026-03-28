/// This file tests execution of specific tests at runtime, compiletime or both.

#include <jtest/jtest.hpp>

#include "check.hpp"

#include <algorithm>
#include <format>
#include <source_location>

namespace
{

namespace invalid
{
// These tests should fail compilation in ways we can't catch and diagnose

// Ask to execute at compiletime without constexpr or consteval
// void comptime(jtest::CTContext& ctx) {}
// void comptime(jtest::Context& ctx) {}
// void comptime(std::same_as<jtest::CTContext> auto& ctx) {}
// void comptime(auto& ctx) {}

// Ask to execute at runtime with consteval
// consteval void runtime(jtest::RTContext& ctx) {}
// consteval void runtime(jtest::Context& ctx) {}
// consteval void runtime(std::same_as<jtest::RTContext> auto& ctx) {}
// consteval void runtime(auto& ctx) {}

} // namespace invalid

void test_invalid()
{
    auto invalid_group = jtest::group_tests<^^invalid>();
    REQUIRE(invalid_group.get_tests().size() == 0,
            "There should not be tests in the invalid group, uncommenting them should cause compiler errors");
}

namespace valid
{
// Constexpr can request anything:
constexpr void cx_both_1(jtest::Context& ctx) {}
constexpr void cx_both_2(auto& ctx) {}
constexpr void cx_ct_1(jtest::CTContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void cx_ct_2(std::same_as<jtest::CTContext> auto& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void cx_rt_1(jtest::RTContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
constexpr void cx_rt_2(std::same_as<jtest::RTContext> auto& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}

// Consteval can request only compiletime execution:
consteval void ce_1(jtest::CTContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
consteval void ce_2(std::same_as<jtest::CTContext> auto& ctx)
{
    ctx.check(std::is_constant_evaluated());
}

// Without consteval or constexpr we can only request runtime execution:
void rt_1(jtest::RTContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
void rt_2(std::same_as<jtest::RTContext> auto& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
} // namespace valid

void test_valid()
{
    auto valid_group = jtest::group_tests<^^valid>();
    auto results = jtest::run_group(valid_group);
    REQUIRE(results.tests.size() == 10, "Unexpected number of tests ran");
    for (const auto& [name, result] : results.tests)
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
            REQUIRE(ct->errors.empty(), std::format("Compiletime tests should be successful for {}", name));
        }
        if (rt)
        {
            REQUIRE(rt->errors.empty(), std::format("Runtime tests should be successful for {}", name));
        }
    }
}

} // namespace

void test_execution()
{
    test_invalid();
    test_valid();
}
