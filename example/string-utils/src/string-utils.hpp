#pragma once

#include <meta>

namespace string_utils
{

constexpr unsigned long long strlen(const char* str)
{
    if (!str)
    {
        throw "Nullptr";
    }

    const char* pos = str;
    for (; *pos; ++pos)
        ;
    return pos - str;
}

consteval std::string_view name_of(std::meta::info info)
{
    if (has_identifier(info))
    {
        return identifier_of(info);
    }
    else
    {
        return display_string_of(info);
    }
}

template<typename T>
constexpr std::string_view name_of()
{
    static constexpr auto name = std::define_static_string(name_of(^^T));
    return name;
}

inline std::string_view name_of(const std::type_info& info)
{
    return info.name();
}

} // namespace string_utils