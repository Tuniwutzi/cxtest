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

inline namespace annotations
{

namespace detail
{

template<size_t n>
struct TemplatedTestcase
{
    std::array<std::meta::info, n> values;
};
template<auto templated_testcase>
constexpr std::span<const std::meta::info> extract_templated_values()
{
    return templated_testcase.values;
}

} // namespace detail

template<typename... Values>
consteval auto templated(Values... values)
{
    if (sizeof...(values) == 0)
    {
        throw std::runtime_error{"Must contain at least one value"};
    }
    auto make_info = []<typename T>(T value)
    {
        if constexpr (std::same_as<std::meta::info, T>)
        {
            return value;
        }
        else
        {
            return std::meta::reflect_constant(value);
        }
    };
    return detail::TemplatedTestcase<sizeof...(values)>{{make_info(std::move(values))...}};
}

} // namespace annotations

constexpr std::string_view version = [] consteval
{
    auto string = std::to_array<char>({
#embed CXTEST_VERSION_FILE
    });
    return std::define_static_string(string);
}();

struct TestOutputSink
{
    constexpr virtual ~TestOutputSink() = default;

    constexpr virtual void record_failure(std::string_view message) = 0;
    constexpr virtual void end_test() = 0;
};

struct GroupOutputSink
{
    constexpr virtual ~GroupOutputSink() = default;

    constexpr virtual TestOutputSink& start_test(std::string_view name, bool compiletime) = 0;
    constexpr virtual void end_group() = 0;
};

struct RunOutputSink
{
    constexpr virtual ~RunOutputSink() = default;
    constexpr virtual GroupOutputSink& start_group(std::string_view name, size_t tests) = 0;
};

struct PrintingRunOutputSink : RunOutputSink, GroupOutputSink, TestOutputSink
{
    bool failed = false;

    GroupOutputSink& start_group(std::string_view name, size_t tests) final;

    TestOutputSink& start_test(std::string_view name, bool compiletime) final;
    void end_group() final;

    void record_failure(std::string_view message) final;
    void end_test() final;
};

struct CollectingTestOutputSink : TestOutputSink
{
    std::vector<std::string> failures;
    constexpr void record_failure(std::string_view message) final
    {
        failures.emplace_back(message);
    }
    constexpr void end_test() final {}
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
    constexpr void end_group() final {}
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

        return std::string{"Check failed at "} + loc.file_name() + ":" + line;
    }

    template<bool throws>
    constexpr void handle_failure(std::string&& message) noexcept(!throws)
    {
        _sink.record_failure(message);
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
    std::optional<std::span<const char* const>> compiletime_failures = {};
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
    catch (const std::exception& ex)
    {
        sink.record_failure(std::string{"Exception escaped the test function: "} + ex.what());
    }
    catch (...)
    {
        sink.record_failure("Exception of unknown type escaped the test function");
    }
}

consteval std::vector<std::meta::info> get_template_values(std::meta::info scope)
{
    std::vector<std::meta::info> values;

    auto annotations = annotations_of(scope) |
                       std::views::filter(
                           [](auto& annotation)
                           {
                               return has_template_arguments(type_of(annotation)) &&
                                      template_of(type_of(annotation)) == ^^annotations::detail::TemplatedTestcase;
                           }) |
                       std::ranges::to<std::vector>();
    if (annotations.size() > 1)
    {
        throw std::meta::exception{
            "More than one templated attribute found on given scope",
            scope,
        };
    }

    if (!annotations.empty())
    {
        auto function = extract<std::span<const std::meta::info> (*)()>(
            substitute(^^annotations::detail::extract_templated_values, {constant_of(annotations.front())}));
        values.append_range(function());
    }

    if (has_parent(scope))
    {
        auto parent_values = get_template_values(parent_of(scope));
        if (!values.empty() && !parent_values.empty())
        {
            throw std::meta::exception{
                "Ambiguous templated values found, annotation is present multiple times in namespace hierarchy",
                scope,
            };
        }
        values.append_range(std::move(parent_values));
    }

    return values;
}

consteval std::string make_test_name(std::meta::info test)
{
    if (has_identifier(test))
    {
        return std::string{identifier_of(test)};
    }
    else if (has_template_arguments(test))
    {
        auto tmpl = template_of(test);
        if (!has_identifier(tmpl))
        {
            throw std::meta::exception{
                "Cannot construct name for a test function whose template has no identifier",
                test,
            };
        }

        std::string name{identifier_of(tmpl)};
        name += "<";
        for (auto& arg : template_arguments_of(test))
        {
            name += display_string_of(arg);
            name += ", ";
        }
        name.resize(name.size() - 1);
        name.back() = '>';
        return name;
    }
    else
    {
        throw std::meta::exception{
            "Cannot construct name for a test function that has no identifier and is not a templated function",
            test,
        };
    }
}

template<std::meta::info info>
    requires(is_function(info))
std::optional<Test> test_from_function()
{
    detail::Test test;
    if constexpr (std::is_invocable_v<decltype([:info:]), CTContext&>)
    {
        constexpr auto result = [&] consteval
        {
            CollectingTestOutputSink sink;
            detail::execute_test<CTContext>([:info:], sink);
            return std::define_static_array(sink.failures | std::views::transform(
                                                                [](auto& string)
                                                                {
                                                                    return std::define_static_string(string);
                                                                }));
        }();
        test.compiletime_failures = result;
    }
    if constexpr (std::is_invocable_v<decltype([:info:]), RTContext&>)
    {
        static constexpr detail::RuntimeTestImpl<([:info:])> runner{};
        test.runtime_test = &runner;
    }

    if (test.compiletime_failures || test.runtime_test)
    {
        test.name = std::define_static_string(make_test_name(info));
        return test;
    }

    return std::nullopt;
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
            if (auto test = test_from_function<info>())
            {
                tests.push_back(std::move(*test));
            }
        }
        else if constexpr (is_function_template(info))
        {
            static constexpr auto templated_values = std::define_static_array(get_template_values(parent_of(info)));
            static_assert(!templated_values.empty(), "Found templated test function without matching annotation");

            template for (constexpr auto& value : templated_values)
            {
                if (!can_substitute(info, {value}))
                {
                    throw std::runtime_error{
                        std::format("Cannot substitute into test template: {}<{}>() is invalid",
                                    display_string_of(info),
                                    display_string_of(value)),
                    };
                }

                if (auto test = test_from_function<substitute(info, {value})>())
                {
                    tests.push_back(std::move(*test));
                }
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
    const std::vector<detail::Test>& get_tests() const noexcept;

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

    auto parent = get_full_namespace(parent_of(info));
    auto child = has_identifier(info) ? identifier_of(info) : std::string_view{};

    if (!parent.empty() && !child.empty())
    {
        parent += "::";
    }
    parent += child;
    return parent;
}

namespace concepts
{

template<std::meta::info candidate>
concept groupable_namespace = is_namespace(candidate) && has_identifier(candidate);

}

template<std::meta::info ns>
    requires(concepts::groupable_namespace<ns>)
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
    if constexpr (!has_identifier(ns))
    {
        throw std::runtime_error{
            "Anonymous namespaces are not allowed within a test group namespace",
        };
    }

    std::vector<Group> groups{};
    if constexpr (concepts::groupable_namespace<ns>)
    {
        if (auto enclosing = group_from_namespace<ns>(); !enclosing.get_tests().empty())
        {
            groups.push_back(std::move(enclosing));
        }
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
    requires(detail::concepts::groupable_namespace<ns>)
Registration register_tests_in_namespace_recursive();

struct [[nodiscard]] Registration
{
    ~Registration();

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    Registration(Registration&&) = delete;
    Registration& operator=(Registration&&) = delete;

    template<std::meta::info ns>
        requires(detail::concepts::groupable_namespace<ns>)
    friend Registration register_tests_in_namespace_recursive();

private:
    Registration(std::vector<detail::Group>&& groups);
    std::list<std::vector<detail::Group>>::const_iterator position;
};

template<std::meta::info ns>
    requires(detail::concepts::groupable_namespace<ns>)
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
