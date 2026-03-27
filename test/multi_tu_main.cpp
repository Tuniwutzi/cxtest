#include <jtest/jtest.hpp>

#include "check.hpp"

#include <algorithm>

namespace
{

namespace multi_tu_main
{

void main_1(jtest::RuntimeTestContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
consteval void main_2(jtest::CompiletimeTestContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void main_3(auto& ctx)
{
    ctx.check(false);
}

} // namespace multi_tu_main

jtest::TestGroup<^^multi_tu_main> group{"multi_tu_main"};

} // namespace

void test_multi_tu()
{
    auto run = jtest::run_all();
    REQUIRE(run.group_results.size() == 2, "Expected registered tests accross multiple TUs to be found");

    auto get_test = [&](auto& results, const std::string& name) -> auto&
    {
        auto pos = results.find(name);
        REQUIRE(pos != results.end(), std::format("Results must contain {}", name));
        return *pos;
    };
    auto check_success = [](const std::pair<std::string, jtest::results::Test>& pair)
    {
        auto& [name, result] = pair;
        REQUIRE((!result.ct_result || result.ct_result->errors.empty()) &&
                    (!result.rt_result || result.rt_result->errors.empty()),
                std::format("Test {} must succeed", name));
    };

    auto main_pos = run.group_results.find("multi_tu_main");
    REQUIRE(main_pos != run.group_results.end(), "Must contain main group");
    REQUIRE(main_pos->second.test_results.size() == 3, "Must contain 3 tests from main group");
    check_success(get_test(main_pos->second.test_results, "main_1"));
    check_success(get_test(main_pos->second.test_results, "main_2"));
    auto& main_3 = get_test(main_pos->second.test_results, "main_3");
    REQUIRE(main_3.second.ct_result && main_3.second.ct_result->errors.size() == 1 && main_3.second.rt_result &&
                main_3.second.rt_result->errors.size() == 1,
            "Test main_3 must fail");

    auto side_pos = run.group_results.find("multi_tu_side");
    REQUIRE(side_pos != run.group_results.end(), "Must contain side group");
    REQUIRE(side_pos->second.test_results.size() == 3, "Must contain 3 tests from side group");
    check_success(get_test(side_pos->second.test_results, "side_1"));
    check_success(get_test(side_pos->second.test_results, "side_2"));
    auto& side_3 = get_test(side_pos->second.test_results, "side_3");
    REQUIRE(side_3.second.ct_result && side_3.second.ct_result->errors.size() == 1 && side_3.second.rt_result &&
                side_3.second.rt_result->errors.size() == 1,
            "Test side_3 must fail");
}