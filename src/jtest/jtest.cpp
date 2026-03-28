#include "jtest.hpp"

#include <iostream>

namespace jtest
{

RTContext::RTContext() {}
const detail::TestResult& RTContext::get_result() const noexcept
{
    return result;
}

namespace detail
{

std::list<Group> registrations{};

} // namespace detail

namespace
{

template<typename T>
void print_test_result(const char* name, const T& result)
{
    if (result.errors.empty())
    {
        return;
    }

    static constexpr std::string_view execution = std::same_as<T, detail::TestResult>              ? "runtime"
                                                  : std::same_as<T, detail::CompiletimeTestResult> ? "compiletime"
                                                                                                   : "??";

    std::cout << std::format("Test {} failed at {} with errors:\n", name, execution);
    for (auto& error : result.errors)
    {
        std::cout << std::format("\t{}\n", error);
    }
}

} // namespace

results::Group run_group(const Group& group) noexcept
{
    results::Group results{};

    auto tests = group.get_tests();
    for (auto& test : tests)
    {
        results::Test test_result{};
        if (auto& result = test.compiletime_result)
        {
            test_result.ct_result = *result;
        }

        if (auto* runtime_test = test.runtime_test)
        {
            test_result.rt_result = detail::execute_test<RTContext>(*runtime_test);
        }

        results.test_results.try_emplace(test.name, std::move(test_result));
    }

    return results;
}

results::Run run_all() noexcept
{
    results::Run run_results{};

    for (auto& group : detail::registrations)
    {
        run_results.group_results.try_emplace(std::string{group.get_name()}, run_group(group));
    }

    return run_results;
}

} // namespace jtest