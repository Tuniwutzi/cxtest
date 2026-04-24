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
        std::cout << std::format("\tCT execution of {}...", name);
    }
    else
    {
        std::cout << std::format("\tRT execution of {}...", name);
    }
    return *this;
}
void PrintingRunOutputSink::end_group() {}
void PrintingRunOutputSink::record_failure(std::string_view message)
{
    if (!failed)
    {
        std::cout << " failed" << std::endl;
        failed = true;
    }
    std::cout << "\t\t" << message << std::endl;
}
void PrintingRunOutputSink::end_test()
{
    if (!failed)
    {
        std::cout << " succeeded" << std::endl;
    }
    failed = false;
}

RTContext::RTContext(TestOutputSink& sink)
    : Context{sink}
{
}

namespace detail
{

std::list<std::vector<Group>> registrations{};

Group::Group(std::string_view name, std::vector<Test> tests)
    : name{std::move(name)}
    , tests{std::move(tests)}
{
}

std::string_view Group::get_name() const noexcept
{
    return name;
}

size_t Group::get_test_count() const noexcept
{
    return tests.size();
}

void Group::run(GroupOutputSink& sink) const noexcept
{
    for (const auto& test : tests)
    {
        if (test.compiletime_failures)
        {
            auto& test_sink = sink.start_test(test.name, true);
            for (const auto& error : *test.compiletime_failures)
            {
                test_sink.record_failure(error);
            }
            test_sink.end_test();
        }
        if (test.runtime_test)
        {
            auto& test_sink = sink.start_test(test.name, false);
            detail::execute_test<RTContext>(*test.runtime_test, test_sink);
            test_sink.end_test();
        }
    }
}

} // namespace detail

Registration::Registration(std::vector<detail::Group>&& groups)
    : position(detail::registrations.insert(detail::registrations.end(), std::move(groups)))
{
}
Registration::~Registration()
{
    detail::registrations.erase(position);
}

void run_registered_tests(RunOutputSink& sink) noexcept
{
    for (const auto& groups : detail::registrations)
    {
        for (const auto& group : groups)
        {
            auto& group_sink = sink.start_group(group.get_name(), group.get_test_count());
            group.run(group_sink);
            group_sink.end_group();
        }
    }
}

} // namespace cxtest