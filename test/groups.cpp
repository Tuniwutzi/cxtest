#include <cxtest/cxtest.hpp>

#include "check.hpp"

namespace group1
{

void group1_test1(cxtest::RTContext&) {}
void group1_test2(cxtest::RTContext&) {}

} // namespace group1

namespace group2
{

void group2_test1(cxtest::RTContext&) {}
void group2_test2(cxtest::RTContext&) {}

namespace group3
{

void group3_test1(cxtest::RTContext&) {}
void group3_test2(cxtest::RTContext&) {}

} // namespace group3

namespace
{

void group2_test3(cxtest::RTContext&) {}
void group2_test4(cxtest::RTContext&) {}

namespace group4
{

void group4_test1(cxtest::RTContext&) {}
void group4_test2(cxtest::RTContext&) {}

} // namespace group4

} // namespace

} // namespace group2

void test_groups()
{
    auto groups1 = cxtest::detail::discover_groups_recursive<^^group1>();
    REQUIRE(groups1.size() == 1, "Expected to discover 1 group from namespace ::group1");
    REQUIRE(groups1.front().get_name() == "group1", "Unexpected name for group from ::group1");
    auto& group1_tests = groups1.front().get_tests();
    REQUIRE(group1_tests.size() == 2, "Unexpected test count in ::group1");
    REQUIRE(group1_tests[0].name == std::string_view{"group1_test1"}, "Unexpected test in ::group1");
    REQUIRE(group1_tests[1].name == std::string_view{"group1_test2"}, "Unexpected test in ::group1");

    auto groups2 = cxtest::detail::discover_groups_recursive<^^group2>();
    REQUIRE(groups2.size() == 3, "Expected to discover 3 groups from ::group2");

    auto& group2 = groups2[0];
    REQUIRE(group2.get_name() == "group2", "Unexpected name");
    auto& group2_tests = group2.get_tests();
    REQUIRE(group2_tests.size() == 4, "Unexpected number of tests");
    REQUIRE(group2_tests[0].name == std::string_view{"group2_test1"}, "Unexpected test name");
    REQUIRE(group2_tests[1].name == std::string_view{"group2_test2"}, "Unexpected test name");
    REQUIRE(group2_tests[2].name == std::string_view{"group2_test3"}, "Unexpected test name");
    REQUIRE(group2_tests[3].name == std::string_view{"group2_test4"}, "Unexpected test name");

    auto& group3 = groups2[1];
    REQUIRE(group3.get_name() == "group2::group3", "Unexpected name");
    auto& group3_tests = group3.get_tests();
    REQUIRE(group3_tests.size() == 2, "Unexpected number of tests");
    REQUIRE(group3_tests[0].name == std::string_view{"group3_test1"}, "Unexpected test name");
    REQUIRE(group3_tests[1].name == std::string_view{"group3_test2"}, "Unexpected test name");

    auto& group4 = groups2[2];
    REQUIRE(group4.get_name() == "group2::group4", "Unexpected name");
    auto& group4_tests = group4.get_tests();
    REQUIRE(group4_tests.size() == 2, "Unexpected number of tests");
    REQUIRE(group4_tests[0].name == std::string_view{"group4_test1"}, "Unexpected test name");
    REQUIRE(group4_tests[1].name == std::string_view{"group4_test2"}, "Unexpected test name");
}
