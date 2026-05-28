#pragma once

#include <format>
#include <functional>
#include <list>
#include <meta>
#include <ranges>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace cxtest
{

constexpr std::string_view version = [] consteval
{
    auto string = std::to_array<char>({
#embed CXTEST_VERSION_FILE
    });
    return std::define_static_string(string);
}();

inline namespace annotations
{

consteval auto test_group()
{
    struct TestGroupAnnotation
    {
    };
    return TestGroupAnnotation{};
}

} // namespace annotations

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

struct Test
{
    const char* name;
    std::optional<std::span<const char* const>> compiletime_failures = {};
    void (*runtime_test)(TestOutputSink&) = nullptr;
};

template<typename Context, std::meta::info test>
constexpr void execute_test(TestOutputSink& sink)
{
    Context context{sink};
    try
    {
        [:test:](context);
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

template<std::meta::info info>
    requires(is_function(info))
Test test_from_function()
{
    detail::Test test;
    constexpr bool ct = std::is_invocable_v<decltype([:info:]), CTContext&>;
    constexpr bool rt = std::is_invocable_v<decltype([:info:]), RTContext&>;

    static_assert(rt || ct, "Encountered a test function that is not callable at runtime or compile time");

    if constexpr (ct)
    {
        constexpr auto result = [&] consteval
        {
            CollectingTestOutputSink sink;
            detail::execute_test<CTContext, info>(sink);
            return std::define_static_array(sink.failures | std::views::transform(
                                                                [](auto& string)
                                                                {
                                                                    return std::define_static_string(string);
                                                                }));
        }();
        test.compiletime_failures = result;
    }
    if constexpr (rt)
    {
        test.runtime_test = execute_test<RTContext, info>;
    }

    test.name = std::define_static_string(identifier_of(info));
    return test;
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

template<size_t n>
consteval std::meta::info make_array_constant(std::span<const std::meta::info> range)
{
    if (range.size() != n)
    {
        throw std::runtime_error{"Range does not have expected size"};
    }
    std::array<std::meta::info, n> arr;
    std::ranges::move(range, arr.begin());
    return std::meta::reflect_constant(arr);
}
template<size_t n, std::meta::info info>
consteval std::vector<std::meta::info> extract_array_constant()
{
    auto arr = extract<std::array<std::meta::info, n>>(info);
    return {std::from_range, arr};
}

struct DiscoveredGroup
{
    const std::meta::info ns;

    const std::meta::info tests;
    const size_t test_count;

    consteval std::vector<std::meta::info> get_tests() const
    {
        auto extract_refl = substitute(^^extract_array_constant,
                                       {std::meta::reflect_constant(test_count), std::meta::reflect_constant(tests)});
        auto extract_fn = extract<std::vector<std::meta::info> (*)()>(extract_refl);
        return extract_fn();
    }
};
consteval bool is_test(std::meta::info test)
{
    if (!is_function(test))
    {
        return false;
    }

    if (!is_invocable_r_type(^^void, type_of(test), {^^RTContext& }) &&
        !is_invocable_r_type(^^void, type_of(test), {^^CTContext& }))
    {
        return false;
    }

    return true;
}
consteval bool is_test_group(std::meta::info ns)
{
    auto annotations = annotations_of_with_type(ns, return_type_of(^^annotations::test_group));

    if (annotations.size() > 1)
    {
        throw std::meta::exception{
            "Namespace has more than one test_group annotation",
            ns,
        };
    }

    return !annotations.empty();
}

consteval std::pair<std::optional<DiscoveredGroup>, std::vector<std::meta::info>>
discover_group_namespace(std::meta::info ns)
{
    bool group = is_test_group(ns);
    if (!has_identifier(ns))
    {
        if (group)
        {
            throw std::meta::exception{
                "A test group cannot be anonymous",
                ns,
            };
        }

        return {{}, {}};
    }

    std::vector<std::meta::info> tests{};
    std::vector<std::meta::info> subgroups{};

    // Current rule: every member of a test group namespace must be...
    // ... a test
    // ... an anonymous namespace which will be ignored
    // ... or another test_group namespace.
    for (auto member : members_of(ns, std::meta::access_context::current()))
    {
        if (is_namespace(member))
        {
            if (!has_identifier(member))
            {
                continue;
            }

            if (group && !is_test_group(member))
            {
                throw std::meta::exception{
                    "Test group has namespace member that is neither anonymous nor a test group",
                    ns,
                };
            }

            subgroups.push_back(member);
        }
        else if (is_test(member))
        {
            tests.push_back(member);
        }
        else if (group)
        {
            throw std::meta::exception{
                std::string{"Test group has member that is neither a test nor a namespace"} + display_string_of(member),
                ns,
            };
        }
    }

    if (group)
    {
        if (tests.empty())
        {
            throw std::meta::exception{
                "Test group contains no tests",
                ns,
            };
        }

        auto make_array_refl = substitute(^^make_array_constant, {std::meta::reflect_constant(tests.size())});
        auto make_array = extract<std::meta::info (*)(std::span<const std::meta::info>)>(make_array_refl);

        DiscoveredGroup group{
            .ns = ns,
            .tests = make_array(tests),
            .test_count = tests.size(),
        };
        return {std::move(group), std::move(subgroups)};
    }
    return {{}, std::move(subgroups)};
}

consteval void discover_groups_recursive(std::meta::info ns, std::vector<DiscoveredGroup>& groups)
{
    auto [group, subgroups] = discover_group_namespace(ns);
    if (group)
    {
        groups.push_back(std::move(*group));
    }
    for (auto subgroup : subgroups)
    {
        discover_groups_recursive(subgroup, groups);
    }
}

template<DiscoveredGroup discovered_group>
Group instantiate_group()
{
    static constexpr std::string_view group_name = std::define_static_string(get_full_namespace(discovered_group.ns));

    std::vector<Test> tests{};
    template for (constexpr std::meta::info test : std::define_static_array(discovered_group.get_tests()))
    {
        tests.push_back(test_from_function<test>());
    }

    return Group{
        group_name,
        std::move(tests),
    };
}

} // namespace detail

struct [[nodiscard]] Registration
{
    ~Registration();

    Registration(const Registration&) = delete;
    Registration& operator=(const Registration&) = delete;
    Registration(Registration&&) = delete;
    Registration& operator=(Registration&&) = delete;

    friend struct RegisterTestsFn;

private:
    Registration(std::vector<detail::Group>&& groups);
    std::list<std::vector<detail::Group>>::const_iterator position;
};

const struct RegisterTestsFn
{
    template<std::meta::info ns = ^^::>
    Registration operator()() const
    {
        std::vector<detail::Group> groups{};
        template for (constexpr auto discovered_group : std::define_static_array(
                          [] consteval
                          {
                              std::vector<detail::DiscoveredGroup> groups{};
                              // auto nss = | std::ranges::to<std::vector>();
                              // throw nss.size();
                              for (auto member : members_of(ns, std::meta::access_context::current()) |
                                                     std::views::filter(std::meta::is_namespace))
                              {
                                  detail::discover_groups_recursive(member, groups);
                              }
                              if (groups.empty())
                              {
                                  throw std::runtime_error{"Register tests found no test groups"};
                              }
                              return groups;
                          }()))
        {
            groups.push_back(detail::instantiate_group<discovered_group>());
        }
        return {std::move(groups)};
    }
} register_tests;

void run_registered_tests(RunOutputSink& sink) noexcept;

} // namespace cxtest
