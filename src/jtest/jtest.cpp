#include "jtest.hpp"

#include <iostream>

namespace jtest
{
RTContext::RTContext(TestOutputSink& sink)
    : Context{sink}
{
}

namespace detail
{

std::list<Group> registrations{};

} // namespace detail

void run_group(const Group& group, GroupOutputSink& sink) noexcept
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


} // namespace jtest