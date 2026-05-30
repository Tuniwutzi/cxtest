#include <cxtest/cxtest.hpp>

#include "check.hpp"

#include <algorithm>

namespace[[= cxtest::test_group()]] multi_tu_main
{

void main_1(cxtest::RTContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
consteval void main_2(cxtest::CTContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void main_3(cxtest::Context& ctx)
{
    ctx.check(false);
}

} // namespace multi_tu_main

namespace
{

auto _ = cxtest::register_tests<^^multi_tu_main>();

} // namespace

void test_multi_tu()
{
    cxtest::CollectingRunOutputSink sink{};
    cxtest::run_registered_tests(sink);
    REQUIRE(sink.groups.size() == 2, "Expected registered tests accross multiple TUs to be found");
    auto& results = sink.groups;

    auto get_test = [&](auto& results, const std::string& name) -> auto&
    {
        auto pos = results.find(name);
        REQUIRE(pos != results.end(), std::format("Results must contain {}", name));
        return *pos;
    };
    auto check_success = [](const std::pair<std::string, cxtest::CollectingGroupOutputSink::TestResult>& pair)
    {
        auto& [name, result] = pair;
        REQUIRE((!result.ct_sink || result.ct_sink->failures.empty()) &&
                    (!result.rt_sink || result.rt_sink->failures.empty()),
                std::format("Test {} must succeed", name));
    };

    auto main_pos = results.find("multi_tu_main");
    REQUIRE(main_pos != results.end(), "Must contain main group");
    REQUIRE(main_pos->second.tests.size() == 3, "Must contain 3 tests from main group");
    check_success(get_test(main_pos->second.tests, "main_1"));
    check_success(get_test(main_pos->second.tests, "main_2"));
    auto& main_3 = get_test(main_pos->second.tests, "main_3");
    REQUIRE(main_3.second.ct_sink && main_3.second.ct_sink->failures.size() == 1 && main_3.second.rt_sink &&
                main_3.second.rt_sink->failures.size() == 1,
            "Test main_3 must fail");

    auto side_pos = results.find("multi_tu_side");
    REQUIRE(side_pos != results.end(), "Must contain side group");
    REQUIRE(side_pos->second.tests.size() == 3, "Must contain 3 tests from side group");
    check_success(get_test(side_pos->second.tests, "side_1"));
    check_success(get_test(side_pos->second.tests, "side_2"));
    auto& side_3 = get_test(side_pos->second.tests, "side_3");
    REQUIRE(side_3.second.ct_sink && side_3.second.ct_sink->failures.size() == 1 && side_3.second.rt_sink &&
                side_3.second.rt_sink->failures.size() == 1,
            "Test side_3 must fail");
}