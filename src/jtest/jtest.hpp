#pragma once

#include <charconv>
#include <format>
#include <functional>
#include <iostream>
#include <list>
#include <meta>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace jtest
{

struct TestOutputSink
{
    constexpr virtual ~TestOutputSink() = default;

    constexpr virtual void error(std::string_view message) = 0;
};

struct GroupOutputSink
{
    constexpr virtual ~GroupOutputSink() = default;

    virtual TestOutputSink& start_test(std::string_view name, bool compiletime) = 0;
};

struct RunOutputSink
{
    constexpr virtual ~RunOutputSink() = default;
    virtual GroupOutputSink& start_group(std::string_view name, size_t tests) = 0;
};

struct PrintingRunOutputSink : RunOutputSink, GroupOutputSink, TestOutputSink
{
    bool failed = false;
    GroupOutputSink& start_group(std::string_view name, size_t tests) final
    {
        std::cout << std::format("Executing group {} with {} tests", name, tests) << std::endl;
        return *this;
    }
    TestOutputSink& start_test(std::string_view name, bool compiletime) final
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
    constexpr void error(std::string_view message) final
    {
        failed = true;
        std::cout << "\t" << message << std::endl;
    }
};

struct CollectingTestOutputSink : TestOutputSink
{
    std::vector<std::string> errors;
    constexpr void error(std::string_view message) final
    {
        errors.emplace_back(message);
    }
};

struct CollectingGroupOutputSink : GroupOutputSink
{
    struct TestResult
    {
        std::optional<CollectingTestOutputSink> ct_sink;
        std::optional<CollectingTestOutputSink> rt_sink;
    };
    std::unordered_map<std::string, TestResult> tests;

    constexpr TestOutputSink& start_test(std::string_view name, bool compiletime) final
    {
        auto& test = tests[std::string{name}];

        if (compiletime)
        {
            return test.ct_sink.emplace();
        }
        else
        {
            return test.rt_sink.emplace();
        }
    }
};

struct CollectingRunOutputSink : RunOutputSink
{
    std::unordered_map<std::string, CollectingGroupOutputSink> groups;

    constexpr GroupOutputSink& start_group(std::string_view name, size_t tests) final
    {
        return groups[std::string{name}];
    }
};

namespace detail
{

struct CompiletimeResults
{
    std::span<const char* const> errors;
};

struct RequireFailed
{
};

} // namespace detail

class Context
{
private:
    static constexpr std::string default_message(std::source_location loc = std::source_location::current())
    {
        std::string line;
        std::to_chars_result result;
        do
        {
            line.resize(line.size() + 32);
            result = std::to_chars(line.data(), line.data() + line.size(), loc.line());
        } while (result.ec == std::errc::value_too_large);

        if (result.ec != std::errc{})
        {
            throw std::runtime_error{"Could not stringify line number"};
        }
        line.resize(result.ptr - line.data());

        return std::string{"Assertion failed at "} + loc.file_name() + ":" + line;
    }

    template<bool throws>
    constexpr void handle_failure(std::string&& message) noexcept(!throws)
    {
        _sink.error(message);
        if constexpr (throws)
        {
            throw detail::RequireFailed{};
        }
    }

public:
    virtual ~Context() = default;

    constexpr void check(bool condition, std::string message = default_message()) noexcept
    {
        if (!condition)
        {
            handle_failure<false>(std::move(message));
        }
    }
    constexpr void check_nothrow(auto&& functor, std::string message = default_message()) noexcept
    {
        try
        {
            std::invoke(std::forward<decltype(functor)>(functor));
        }
        catch (...)
        {
            handle_failure<false>(std::move(message));
        }
    }
    constexpr void check_throws(auto&& functor, std::string message = default_message()) noexcept
    {
        try
        {
            std::invoke(std::forward<decltype(functor)>(functor));
            handle_failure<false>(std::move(message));
        }
        catch (...)
        {
        }
    }

    constexpr void require(bool condition, std::string message = default_message())
    {
        if (!condition)
        {
            handle_failure<true>(std::move(message));
        }
    }
    constexpr void require_nothrow(auto&& functor, std::string message = default_message())
    {
        try
        {
            std::invoke(std::forward<decltype(functor)>(functor));
        }
        catch (...)
        {
            handle_failure<true>(std::move(message));
        }
    }
    constexpr void require_throws(auto&& functor, std::string message = default_message())
    {
        bool threw = false;
        try
        {
            std::invoke(std::forward<decltype(functor)>(functor));
        }
        catch (...)
        {
            threw = true;
        }
        if (!threw)
        {
            handle_failure<true>(std::move(message));
        }
    }

protected:
    constexpr Context(TestOutputSink& sink)
        : _sink(sink)
    {
    }

    TestOutputSink& _sink;
};

class CTContext : public Context
{
public:
    // Make sure it can only be constructed at compiletime
    consteval CTContext(TestOutputSink& sink)
        : Context{sink}
    {
    }
};

class RTContext : public Context
{
public:
    // Make sure it can only be constructed at runtime
    RTContext(TestOutputSink& sink);
};

namespace detail
{

struct RuntimeTest
{
public:
    virtual ~RuntimeTest() = default;
    virtual void operator()(RTContext& context) const = 0;
};

template<auto function>
struct RuntimeTestImpl : RuntimeTest
{
    void operator()(RTContext& context) const override
    {
        function(context);
    }
};

struct Test
{
    const char* name;
    std::optional<std::span<const char* const>> compiletime_errors = {};
    const RuntimeTest* runtime_test = nullptr;
};

template<typename Context>
constexpr void execute_test(auto&& test, TestOutputSink& sink)
{
    Context context{sink};
    try
    {
        test(context);
    }
    catch (const detail::RequireFailed&)
    {
    }
}

template<std::meta::info info>
void discover_tests(std::string_view group_name, std::vector<Test>& tests)
{
    if constexpr (is_function(info))
    {
        detail::Test test;
        if constexpr (std::is_invocable_v<decltype([:info:]), CTContext&>)
        {
            constexpr auto result = [&] consteval
            {
                CollectingTestOutputSink sink;
                detail::execute_test<CTContext>([:info:], sink);
                return std::define_static_array(sink.errors | std::views::transform(
                                                                  [](auto& string)
                                                                  {
                                                                      return std::define_static_string(string);
                                                                  }));
            }();
            test.compiletime_errors = result;
        }
        if constexpr (std::is_invocable_v<decltype([:info:]), RTContext&>)
        {
            static constexpr detail::RuntimeTestImpl<([:info:])> runner{};
            test.runtime_test = &runner;
        }

        if (test.compiletime_errors || test.runtime_test)
        {
            test.name = std::define_static_string(identifier_of(info));
            for (const detail::Test& existing : tests)
            {
                if (test.name == existing.name)
                {
                    throw std::runtime_error{
                        std::format("Test with name {} already exists in group {}", test.name, group_name)};
                }
            }
            tests.push_back(std::move(test));
        }
    }
    else if constexpr (is_namespace(info))
    {
        template for (constexpr auto member :
                      std::define_static_array(members_of(info, std::meta::access_context::current())))
        {
            discover_tests<member>(group_name, tests);
        }
    }
}

} // namespace detail

class [[nodiscard]] Group
{
public:
    static std::string default_name(std::source_location loc = std::source_location::current())
    {
        return std::format("{}:{}", loc.file_name(), loc.line());
    }

    Group(std::string name = default_name())
        : name{std::move(name)}
    {
    }

    template<std::meta::info test_or_namespace>
    void add()
    {
        detail::discover_tests<test_or_namespace>(name, tests);
    }

    std::string_view get_name() const noexcept
    {
        return name;
    }

    // TODO: This is not clean; it exposes a detail:: type in a public interface
    std::span<const detail::Test> get_tests() const noexcept
    {
        return tests;
    }

private:
    std::string name;
    std::vector<detail::Test> tests;
};

template<std::meta::info test_or_namespace>
Group group_tests(std::string name = Group::default_name())
{
    Group group{std::move(name)};
    group.add<test_or_namespace>();
    return group;
}

namespace detail
{
extern std::list<Group> registrations;
}

struct [[nodiscard]] Registration
{
    Registration(Group group)
        : position{[&group]
                   {
                       return detail::registrations.insert(detail::registrations.end(), std::move(group));
                   }()}
    {
    }
    ~Registration()
    {
        detail::registrations.erase(position);
    }

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    Registration(Registration&&) = delete;
    Registration& operator=(Registration&&) = delete;

private:
    std::list<Group>::const_iterator position;
};

template<std::meta::info test_or_namespace>
Registration register_tests(std::string name = Group::default_name())
{
    return {group_tests<test_or_namespace>(std::move(name))};
}

void run_group(const Group& group, GroupOutputSink& sink) noexcept;
void run_registered_tests(RunOutputSink& sink) noexcept;

} // namespace jtest