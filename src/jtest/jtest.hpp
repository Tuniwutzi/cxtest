#pragma once

#include <charconv>
#include <format>
#include <functional>
#include <list>
#include <meta>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace jtest
{

namespace results
{

struct Runtime
{
    std::vector<std::string> errors;
};

struct Compiletime
{
    std::span<const char* const> errors;
};

} // namespace results

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

        return std::string{"Assertion failed at "} + loc.file_name() + ":" + line;
    }

    template<bool throws>
    constexpr void handle_failure(std::string&& message) noexcept(!throws)
    {
        result.errors.push_back(std::move(message));
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
    Context() = default;

    results::Runtime result;
};

class CTContext : public Context
{
public:
    consteval CTContext() = default;
    consteval const results::Runtime& get_result() const noexcept
    {
        return result;
    }
};

class RTContext : public Context
{
public:
    RTContext();
    const results::Runtime& get_result() const noexcept;
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
    std::optional<results::Compiletime> compiletime_result = {};
    const RuntimeTest* runtime_test = nullptr;
};

template<typename Context>
constexpr results::Runtime execute_test(auto&& test)
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

template<std::meta::info info>
void discover_tests(std::string_view group_name, std::vector<Test>& tests)
{
    auto add_test = [&](detail::Test test)
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
    };
    if constexpr (is_template(info))
    {
        detail::Test test;
        if constexpr (constexpr auto compiletime_test = [] consteval -> std::optional<std::meta::info>
                      {
                          try
                          {
                              return substitute(info, {^^CTContext});
                          }
                          catch (...)
                          {
                              return std::nullopt;
                          }
                      }())
        {

            constexpr auto result =
                std::define_static_array(detail::execute_test<CTContext>([:*compiletime_test:]).errors |
                                         std::views::transform(
                                             [](auto& string)
                                             {
                                                 return std::define_static_string(string);
                                             }));
            test.compiletime_result = results::Compiletime{.errors = result};
        }

        if constexpr (constexpr auto runtime_test = [] consteval -> std::optional<std::meta::info>
                      {
                          try
                          {
                              return substitute(info, {^^RTContext});
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
        if constexpr (std::is_invocable_v<decltype([:info:]), CTContext&>)
        {
            constexpr auto result = std::define_static_array(detail::execute_test<CTContext>([:info:]).errors |
                                                             std::views::transform(
                                                                 [](auto& string)
                                                                 {
                                                                     return std::define_static_string(string);
                                                                 }));
            test.compiletime_result = results::Compiletime{.errors = result};
        }
        if constexpr (std::is_invocable_v<decltype([:info:]), RTContext&>)
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
            discover_tests<member>(group_name, tests);
        }
    }
}

} // namespace detail

class Group
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

namespace results
{
struct Test
{
    std::optional<Compiletime> compiletime;
    std::optional<Runtime> runtime;
};

struct Group
{
    std::unordered_map<std::string, Test> tests;
};

} // namespace results

results::Group run_group(const Group& group) noexcept;
std::unordered_map<std::string, results::Group> run_registered_tests() noexcept;
void print_results(const results::Test& test);
void print_results(const results::Group& group);
void print_results(const std::unordered_map<std::string, results::Group>& groups);

} // namespace jtest