#pragma once

#include <format>
#include <meta>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace jtest
{

namespace detail
{

struct RequireFailed : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

struct TestResult
{
    std::vector<std::string> errors;
};

struct CompiletimeTestResult
{
    std::span<const char* const> errors;
};

} // namespace detail

class TestContext
{
public:
    virtual ~TestContext() = default;

    constexpr void check(bool condition) noexcept
    {
        if (!condition)
        {
            result.errors.push_back("Error!");
        }
    }
    constexpr void require(bool condition)
    {
        if (!condition)
        {
            result.errors.push_back("Error!");
            throw detail::RequireFailed{"Error!"};
        }
    }

protected:
    TestContext() = default;

    detail::TestResult result;
};

class CompiletimeTestContext : public TestContext
{
public:
    consteval CompiletimeTestContext() = default;
    consteval const detail::TestResult& get_result() const noexcept
    {
        return result;
    }
};

class RuntimeTestContext : public TestContext
{
public:
    RuntimeTestContext();
    const detail::TestResult& get_result() const noexcept;
};

namespace detail
{

struct RuntimeTest
{
public:
    virtual ~RuntimeTest() = default;
    virtual void operator()(RuntimeTestContext& context) const = 0;
};

template<auto function>
struct RuntimeTestImpl : RuntimeTest
{
    void operator()(RuntimeTestContext& context) const override
    {
        function(context);
    }
};

struct Test
{
    const char* name;
    std::optional<detail::CompiletimeTestResult> compiletime_result = {};
    const RuntimeTest* runtime_test = nullptr;
};

struct GroupResults
{
    size_t failed;
};

class BasicTestGroup
{
public:
    virtual ~BasicTestGroup() = default;

    virtual std::string_view get_name() const noexcept = 0;
    virtual std::span<const Test> get_tests() const noexcept = 0;
};

extern std::vector<BasicTestGroup*> test_groups;

template<typename Context>
constexpr detail::TestResult execute_test(auto&& test)
{
    Context context{};
    try
    {
        test(context);
    }
    catch (const detail::RequireFailed&)
    {
    }
    return context.get_result();
}

} // namespace detail

constexpr struct SkipRegistration
{
} skip_registration{};

template<std::meta::info... infos>
class TestGroup : public detail::BasicTestGroup
{
private:
    template<std::meta::info info>
    void discover()
    {
        auto add_test = [this](detail::Test test)
        {
            test.name = std::define_static_string(identifier_of(info));
            for (const detail::Test& existing : tests)
            {
                if (test.name == existing.name)
                {
                    throw std::runtime_error{
                        std::format("Test with name {} already exists in group {}", test.name, this->name)};
                }
            }
            tests.push_back(std::move(test));
        };
        if constexpr (is_template(info))
        {
            detail::Test test;
            if constexpr (constexpr auto compiletime_test = [] consteval -> std::optional<std::meta::info>
                          {
                              try
                              {
                                  return substitute(info, {^^CompiletimeTestContext});
                              }
                              catch (...)
                              {
                                  return std::nullopt;
                              }
                          }())
            {

                constexpr auto result = std::define_static_array(
                    detail::execute_test<CompiletimeTestContext>([:*compiletime_test:]).errors |
                    std::views::transform(
                        [](auto& string)
                        {
                            return std::define_static_string(string);
                        }));
                test.compiletime_result = detail::CompiletimeTestResult{.errors = result};
            }

            if constexpr (constexpr auto runtime_test = [] consteval -> std::optional<std::meta::info>
                          {
                              try
                              {
                                  return substitute(info, {^^RuntimeTestContext});
                              }
                              catch (...)
                              {
                                  return std::nullopt;
                              }
                          }())
            {
                static constexpr detail::RuntimeTestImpl<([:*runtime_test:])> runner{};
                test.runtime_test = &runner;
            }

            if (test.compiletime_result || test.runtime_test)
            {
                add_test(std::move(test));
            }
        }
        else if constexpr (is_function(info))
        {
            detail::Test test;
            if constexpr (std::is_invocable_v<decltype([:info:]), CompiletimeTestContext&>)
            {
                constexpr auto result =
                    std::define_static_array(detail::execute_test<CompiletimeTestContext>([:info:]).errors |
                                             std::views::transform(
                                                 [](auto& string)
                                                 {
                                                     return std::define_static_string(string);
                                                 }));
                test.compiletime_result = detail::CompiletimeTestResult{.errors = result};
            }
            if constexpr (std::is_invocable_v<decltype([:info:]), RuntimeTestContext&>)
            {
                static constexpr detail::RuntimeTestImpl<([:info:])> runner{};
                test.runtime_test = &runner;
            }

            if (test.compiletime_result || test.runtime_test)
            {
                add_test(std::move(test));
            }
        }
        else if constexpr (is_namespace(info))
        {
            template for (constexpr auto member :
                          std::define_static_array(members_of(info, std::meta::access_context::current())))
            {
                discover<member>();
            }
        }
    }

    static std::string default_name(std::source_location loc = std::source_location::current())
    {
        return std::format("{}:{}", loc.file_name(), loc.line());
    }

public:
    TestGroup(SkipRegistration, std::string name = default_name())
        : name{std::move(name)}
    {
        ((discover<infos>()), ...);
    }
    TestGroup(std::string name = default_name())
        : TestGroup(skip_registration, std::move(name))
    {
        for (auto& group : detail::test_groups)
        {
            if (group->get_name() == name)
            {
                throw std::invalid_argument{std::format("A test group named {} already exists", name)};
            }
        }

        detail::test_groups.push_back(this);
    }

    std::string_view get_name() const noexcept override
    {
        return name;
    }

    std::span<const detail::Test> get_tests() const noexcept override
    {
        return tests;
    }

private:
    std::string name;
    std::vector<detail::Test> tests;
};

namespace results
{
struct Test
{
    // TODO: this is bad, organize this differently
    std::optional<detail::CompiletimeTestResult> ct_result;
    std::optional<detail::TestResult> rt_result;
};

struct Group
{
    std::unordered_map<std::string, Test> test_results;
};

struct Run
{
    std::unordered_map<std::string, Group> group_results;
};

} // namespace results

// TODO: move the generic test group out of detail:: for this?
results::Group run_group(const detail::BasicTestGroup& group) noexcept;
results::Run run_all() noexcept;
void print_results(const results::Test& test);
void print_results(const results::Group& group);
void print_results(const results::Run& run);
bool is_result_successful(const results::Test& test);
bool is_result_successful(const results::Group& test);
bool is_result_successful(const results::Run& test);

} // namespace jtest