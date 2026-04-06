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

class [[nodiscard]] Group
{
public:
    Group(std::string name);

    std::string_view get_name() const noexcept;

    std::span<const detail::Test> get_tests() const noexcept;
    void run(GroupOutputSink& sink) const noexcept;

    template<std::meta::info info>
        requires(is_namespace(info))
    static Group from_namespace()
    {
        Group rv{{identifier_of(info)}};
        auto functions =
            members_of(info, std::meta::access_context::current()) | std::views::filter(
                                                                         [](auto member)
                                                                         {
                                                                             if (!is_function(member))
                                                                             {
                                                                                 return false;
                                                                             }

                                                                             if (return_type_of(member) != ^^void)
                                                                             {
                                                                                 return false;
                                                                             }

                                                                             return true;
                                                                         });
        for (auto function : functions)
        {
            auto parameters = parameters_of(function);
            if (parameters.size() != 1)
            {
                continue;
            }

            auto parameter = parameters.front();

            bool compiletime = false;
            bool runtime = false;
            if (parameter == ^^(Context&))
            {
                compiletime = true;
                runtime = true;
            }
            else if (parameter == ^^(CTContext&))
            {
                compiletime = true;
            }
            else if (parameter == ^^(RTContext&))
            {
                runtime = true;
            }
            else
            {
                continue;
            }

            Test test{.name = std::define_static_string(identifier_of(member))};
            if (compiletime)
            {
                CollectingTestOutputSink sink;
                detail::execute_test<CTContext>([:member:], sink);
                test.compiletime_errors =
                    std::define_static_array(sink.errors | std::views::transform(
                                                               [](auto& string)
                                                               {
                                                                   return std::define_static_string(string);
                                                               }));
            }
            if (runtime)
            {
                auto executor = substitute(^^execute_runtime_test, {member});
                test.runtime_test = extract<void (*)(RTContext&)>(executor);
            }
            group.tests.push_back(std::move(test));
        }
        return group;
    }
    template<std::meta::info info>
        requires(is_namespace(info))
    static std::vector<Group> from_namespace_recursive()
    {
        std::vector<Group> groups;

        from_namespace_recursive_impl<info>(groups);

        return groups;
    }

private:
    template<std::meta::info info>
    static void from_namespace_recursive_impl(std::vector<Group>& groups)
    {
        auto group = from_namespace<info>();
        if (!group.get_tests().empty())
        {
            groups.push_back(std::move(group));
        }

        constexpr auto namespaces =
            members_of(info, std::meta::access_context::current()) | std::views::filter(
                                                                         [](auto member)
                                                                         {
                                                                             return is_namespace(member);
                                                                         });
        template for (constexpr auto ns_info : namespaces)
        {
            from_namespace_recursive_impl<ns_info>(groups);
        }
    }

    std::string name;
    std::vector<detail::Test> tests;
};

} // namespace detail

struct [[nodiscard]] Registration
{
    Registration(std::vector<detail::Group>&& groups);
    ~Registration();

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    Registration(Registration&&) = delete;
    Registration& operator=(Registration&&) = delete;

private:
    std::vector<std::list<Group>::const_iterator> positions;
};

template<std::meta::info namespace_info>
    requires(is_namespace(namespace_info))
Registration register_tests_in_namespace()
{
    auto group = detail::Group::from_namespace<namespace_info>();
    return {{std::move(group)}};
}
template<std::meta::info namespace_info>
    requires(is_namespace(namespace_info))
Registration register_tests_in_namespace_recursive()
{
    return {detail::Group::from_namespace_recursive<namespace_info>()};
}

void run_registered_tests(RunOutputSink& sink) noexcept;

} // namespace cxtest
