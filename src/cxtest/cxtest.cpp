#include "cxtest.hpp"

#include <iostream>

namespace cxtest
{

GroupOutputSink& PrintingRunOutputSink::start_group(std::string_view name, size_t tests)
{
    std::cout << std::format("Executing group {} with {} tests", name, tests) << std::endl;
    return *this;
}
TestOutputSink& PrintingRunOutputSink::start_test(std::string_view name, bool compiletime)
{
    if (compiletime)
    {
        std::cout << std::format("Results of test {} at compiletime", name) << std::endl;
    }
    else
    {
        std::cout << std::format("Executing test {} at runtime", name) << std::endl;
    }
    return *this;
}
void PrintingRunOutputSink::error(std::string_view message)
{
    failed = true;
    std::cout << "\t" << message << std::endl;
}

RTContext::RTContext(TestOutputSink& sink)
    : Context{sink}
{
}

detail::Group::Group(std::string_view name, std::vector<Test> tests)
    : name{name}
    , tests{std::move(tests)}
{
}

std::string_view detail::Group::get_name() const noexcept
{
    return name;
}

std::span<const detail::Test> detail::Group::get_tests() const noexcept
{
    return tests;
}

namespace detail
{

std::list<Group> registrations{};

} // namespace detail

Registration::Registration(detail::Group&& group)
    : position{detail::registrations.insert(detail::registrations.end(), std::move(group))}
{
}
Registration::~Registration()
{
    detail::registrations.erase(position);
}

void run_group(const detail::Group& group, GroupOutputSink& sink) noexcept
{
    for (const auto& test : group.get_tests())
    {
        if (test.compiletime_errors)
        {
            auto& test_sink = sink.start_test(test.name, true);
            for (const auto& error : *test.compiletime_errors)
            {
                test_sink.error(error);
            }
        }
        if (test.runtime_test)
        {
            auto& test_sink = sink.start_test(test.name, false);
            detail::execute_test<RTContext>(*test.runtime_test, test_sink);
        }
    }
}

void run_registered_tests(RunOutputSink& sink) noexcept
{
    for (const auto& group : detail::registrations)
    {
        const auto& tests = group.get_tests();
        auto& group_sink = sink.start_group(group.get_name(), tests.size());
        run_group(group, group_sink);
    }
}

} // namespace cxtest