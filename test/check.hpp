#pragma once

#include <format>
#include <source_location>
#include <stdexcept>

namespace
{
std::string format_source_location(std::source_location loc = std::source_location::current())
{
    return std::format("{}:{}", loc.file_name(), loc.line());
}

// Helper for testing cxtest
template<std::meta::info ns>
cxtest::detail::Group discover_and_instantiate_group()
{
    constexpr auto group = cxtest::detail::discover_group_namespace(ns).first;
    static_assert(group);
    return cxtest::detail::instantiate_group<*group>();
}

} // namespace
#define REQUIRE(condition, context)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            throw std::runtime_error{std::format("Condition failed at {}: {}", format_source_location(), context)};    \
        }                                                                                                              \
    } while (false)
