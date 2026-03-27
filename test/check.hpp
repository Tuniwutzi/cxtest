#pragma once

#include <format>
#include <source_location>
#include <stdexcept>

std::string format_source_location(std::source_location loc = std::source_location::current())
{
    return std::format("{}:{}", loc.file_name(), loc.line());
}
#define CHECK(condition, context)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(condition))                                                                                              \
        {                                                                                                              \
            throw std::runtime_error{std::format("Condition failed at {}: {}", format_source_location(), context)};    \
        }                                                                                                              \
    } while (false)
