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
// void comptime(jtest::CompiletimeTestContext& ctx) {}
// void comptime(jtest::TestContext& ctx) {}
// void comptime(std::same_as<jtest::CompiletimeTestContext> auto& ctx) {}
// void comptime(auto& ctx) {}

// Ask to execute at runtime with consteval
// consteval void runtime(jtest::RuntimeTestContext& ctx) {}
// consteval void runtime(jtest::TestContext& ctx) {}
// consteval void runtime(std::same_as<jtest::RuntimeTestContext> auto& ctx) {}
// consteval void runtime(auto& ctx) {}

} // namespace invalid

void test_invalid()
{
    jtest::TestGroup<^^invalid> invalid_group{jtest::skip_registration};
    CHECK(invalid_group.get_tests().size() == 0,
          "There should not be tests in the invalid group, uncommenting them should cause compiler errors");
}

namespace valid
{
// Constexpr can request anything:
constexpr void cx_both(jtest::TestContext& ctx) {}
constexpr void cx_both(auto& ctx) {}
constexpr void cx_ct(jtest::CompiletimeTestContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void cx_ct(std::same_as<jtest::CompiletimeTestContext> auto& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void cx_rt(jtest::RuntimeTestContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
constexpr void cx_rt(std::same_as<jtest::RuntimeTestContext> auto& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}

// Consteval can request only compiletime execution:
consteval void ce(jtest::CompiletimeTestContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
consteval void ce(std::same_as<jtest::CompiletimeTestContext> auto& ctx)
{
    ctx.check(std::is_constant_evaluated());
}

// Without consteval or constexpr we can only request runtime execution:
void rt(jtest::RuntimeTestContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
void rt(std::same_as<jtest::RuntimeTestContext> auto& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
} // namespace valid

void test_valid()
{
    jtest::TestGroup<^^valid> valid_group{jtest::skip_registration};
    auto results = jtest::run_group(valid_group);
    CHECK(results.test_results.size() == 10, "Unexpected number of tests ran");
    for (const auto& [name, result] : results.test_results)
    {
        const auto& [ct, rt] = result;
        if (name == "cx_both")
        {
            CHECK(ct && rt, "cx_both should run compiletime and runtime tests");
        }
        else if (name == "cx_ct")
        {
            CHECK(ct && !rt, "cx_ct should run only compiletime tests");
        }
        else if (name == "cx_rt")
        {
            CHECK(!ct && rt, "cx_rt should run only runtime tests");
        }
        else if (name == "ce")
        {
            CHECK(ct && !rt, "ce should run only compiletime tests");
        }
        else if (name == "rt")
        {
            CHECK(!ct && rt, "rt should run only runtime tests");
        }
        else
        {
            CHECK(false, std::format("Unknown test name for {}", name));
        }

        if (ct)
        {
            CHECK(ct->errors.empty(), std::format("Compiletime tests should be successful for {}", name));
        }
        if (rt)
        {
            CHECK(rt->errors.empty(), std::format("Runtime tests should be successful for {}", name));
        }
    }
}

} // namespace

void test_execution()
{
    test_invalid();
    test_valid();
}
