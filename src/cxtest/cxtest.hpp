#pragma once

#include <algorithm>
#include <format>
#include <functional>
#include <list>
#include <memory>
#include <meta>
#include <ranges>
#include <source_location>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace cxtest
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
    GroupOutputSink& start_group(std::string_view name, size_t tests) final;
    TestOutputSink& start_test(std::string_view name, bool compiletime) final;
    void error(std::string_view message) final;
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

template<auto test>
void execute_runtime_test(RTContext& context)
{
    test(context);
}

struct Test
{
    const char* name;
    std::optional<std::span<const char* const>> compiletime_errors = {};
    void (*runtime_test)(RTContext&) = nullptr;
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

struct DiscoveredTest
{
    virtual ~DiscoveredTest() = default;
    // Execute compiletime portion of this test and store a function pointer for runtime execution
    virtual consteval Test to_runtime() const noexcept = 0;
};
template<std::meta::info function>
struct DiscoveredTestsFreeFunction : DiscoveredTest
{
    consteval Test to_runtime() const noexcept final
    {
        Test test{
            .name = std::define_static_string(identifier_of(function)),
        };
        if constexpr (std::ranges::contains(std::array{^^Context&, ^^CTContext& }, parameters_of(function).front()))
        {
            CollectingTestOutputSink sink;
            execute_test<CTContext>([:function:], sink);
            test.compiletime_errors =
                std::define_static_array(sink.errors | std::views::transform(
                                                           [](auto& string)
                                                           {
                                                               return std::define_static_string(string);
                                                           }));
        }
        if constexpr (std::ranges::contains(std::array{^^Context&, ^^RTContext& }, parameters_of(function).front()))
        {
            auto executor = substitute(^^execute_runtime_test, {function});
            test.runtime_test = extract<void (*)(RTContext&)>(executor);
        }
        return test;
    }
};
template<typename T>
constexpr std::unique_ptr<DiscoveredTest> make_discovered_test_implementation()
{
    return std::make_unique<T>();
}

consteval std::vector<std::unique_ptr<DiscoveredTest>> discover_tests_in_namespace(std::meta::info ns)
{
    if (!is_namespace(ns))
    {
        throw std::meta::exception{"Parameter is not a namespace", ns};
    }

    std::vector<std::unique_ptr<DiscoveredTest>> tests;

    for (auto function : members_of(ns, std::meta::access_context::current()))
    {
        if (!is_function(function))
        {
            continue;
        }

        if (return_type_of(function) != ^^void)
        {
            continue;
        }

        auto parameters = parameters_of(function);
        if (parameters.size() != 1)
        {
            continue;
        }

        auto parameter = parameters.front();
        if (!std::ranges::contains(std::array{^^Context&, ^^RTContext&, ^^CTContext& }, parameter))
        {
            continue;
        }

        auto impl_type = substitute(^^DiscoveredTestsFreeFunction, {^^function});
        auto impl_creator = extract<std::unique_ptr<DiscoveredTest> (*)()>(
            substitute(^^make_discovered_test_implementation, {impl_type}));
        tests.push_back(impl_creator());
    }

    return tests;
}

class [[nodiscard]] Group
{
public:
    Group(std::string_view name, std::vector<Test> tests);

    std::string_view get_name() const noexcept;

    std::span<const Test> get_tests() const noexcept;
    void run(GroupOutputSink& sink) const noexcept;

private:
    std::string_view name;
    std::vector<Test> tests;
};

} // namespace detail

struct [[nodiscard]] Registration
{
    // TODO: hide this
    Registration(detail::Group&& group);
    ~Registration();

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    Registration(Registration&&) = delete;
    Registration& operator=(Registration&&) = delete;

private:
    std::list<detail::Group>::const_iterator position;
};

template<std::meta::info namespace_info>
    requires(is_namespace(namespace_info))
Registration register_tests_in_namespace()
{
    constexpr auto discovered_tests = detail::discover_tests_in_namespace(namespace_info);
    static_assert(!discovered_tests.empty(), "Namespace contains no valid test functions");

    return {
        {
            define_static_string(identifier_of(namespace_info)),
            discovered_tests | std::views::transform(
                                   [](auto& discovered_test)
                                   {
                                       return discovered_test.to_runtime();
                                   }),
        },
    };
}

void run_registered_tests(RunOutputSink& sink) noexcept;

} // namespace cxtest
