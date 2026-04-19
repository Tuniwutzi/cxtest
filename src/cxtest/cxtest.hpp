#pragma once

#include <format>
#include <functional>
#include <list>
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

template<std::meta::info ns>
    requires(is_namespace(ns))
std::vector<Test> discover_tests()
{
    std::vector<Test> tests;
    template for (constexpr auto info : std::define_static_array(members_of(ns, std::meta::access_context::current())))
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
                tests.push_back(std::move(test));
            }
        }
    }
    return tests;
}

class [[nodiscard]] Group
{
public:
    Group(std::string_view name, std::vector<Test> tests);

    std::string_view get_name() const noexcept;
    size_t get_test_count() const noexcept;

    void run(GroupOutputSink& sink) const noexcept;

private:
    std::string name;
    std::vector<detail::Test> tests;
};

consteval std::string get_full_namespace(std::meta::info info)
{
    if (!has_parent(info))
    {
        return ""; // global namespace, finish
    }

    auto result = get_full_namespace(parent_of(info));
    if (!result.empty())
    {
        result += "::";
    }

    if (has_identifier(info))
    {
        result += identifier_of(info);
    }
    else
    {
        result += "<anonymous>";
    }
    return result;
}

template<std::meta::info ns>
    requires(is_namespace(ns))
Group group_from_namespace()
{
    auto tests = discover_tests<ns>();

    static constexpr std::string_view group_name = std::define_static_string(get_full_namespace(ns));

    size_t drop = 0;
    for (const detail::Test& test : tests)
    {
        drop++;
        for (const detail::Test& existing : tests | std::views::drop(drop))
        {
            if (test.name == existing.name)
            {
                throw std::runtime_error{
                    std::format("Test with name {} exists multiple times in group {}", test.name, group_name)};
            }
        }
    }

    return Group{
        group_name,
        std::move(tests),
    };
}

template<std::meta::info ns>
    requires(is_namespace(ns))
std::vector<Group> discover_groups_recursive()
{
    std::vector<Group> groups{};
    if (auto enclosing = group_from_namespace<ns>(); enclosing.get_test_count() > 0)
    {
        groups.push_back(std::move(enclosing));
    }

    template for (constexpr auto member :
                  std::define_static_array(members_of(ns, std::meta::access_context::current())))
    {
        if constexpr (is_namespace(member))
        {
            groups.append_range(discover_groups_recursive<member>());
        }
    }

    return groups;
}

} // namespace detail

struct Registration;

template<std::meta::info ns>
    requires(is_namespace(ns))
Registration register_tests_in_namespace_recursive();

struct [[nodiscard]] Registration
{
    ~Registration();

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    Registration(Registration&&) = delete;
    Registration& operator=(Registration&&) = delete;

    template<std::meta::info ns>
        requires(is_namespace(ns))
    friend Registration register_tests_in_namespace_recursive();

private:
    Registration(std::vector<detail::Group>&& groups);
    std::list<std::vector<detail::Group>>::const_iterator position;
};

template<std::meta::info ns>
    requires(is_namespace(ns))
Registration register_tests_in_namespace_recursive()
{
    auto groups = detail::discover_groups_recursive<ns>();
    if (groups.empty())
    {
        throw std::runtime_error{"Namespace does not contain any tests"};
    }
    return Registration{std::move(groups)};
}

void run_registered_tests(RunOutputSink& sink) noexcept;

} // namespace cxtest
