#pragma once

#include "execution.hpp"

namespace cxtest::detail::discovery
{

struct Test
{
    std::string name;
    std::unique_ptr<execution::Executor> executor;
};
struct Group
{
    std::string name;
    std::vector<Test> tests;
};

// Discovers individual tests within a namespace
consteval std::vector<Test> discover_tests_in_namespace(std::meta::info ns)
{
    std::vector<Test> tests;
    for (auto member : members_of(ns, std::meta::access_context::current()))
    {
        if (auto executor = execution::make_executor(member))
        {
            tests.push_back(identifier_of(member), std::move(executor));
        }
    }
    return tests;
}

consteval Group make_group_from_namespace(std::meta::info ns)
{
    return {
        identifier_of(ns),
        discover_tests_in_namespace(ns),
    };
}

// Future: discovers tests within a TestCase
// consteval std::vector<Test> discover_tests_in_test_case(std::meta::info class_inheriting_TestCase)
// Future: makes group from TestCase
// consteval Group make_group_from_test_case(std::meta::info class_inheriting_TestCase)

// Entrypoint:
// "consteval discover(std::meta::info ns)", runs make_group_from_namespace on the passed
// namespace and, recursively, every namespace contained by it. In addition it discovery all TestCase inheritors and
// turns them into groups as well.

// Question: is discover part of the detail namespace?
// Will be answered when we know exactly when tests get transferred to runtim.

} // namespace cxtest::detail::discovery
