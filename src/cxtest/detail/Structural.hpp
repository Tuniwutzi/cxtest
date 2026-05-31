#pragma once

#include <algorithm>
#include <array>
#include <meta>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace cxtest::detail::structural
{

template<typename T>
    requires(is_structural_type(^^T))
struct List
{
    size_t _size;
    std::meta::info _data;

    consteval List()
        : _data{}
        , _size{0}
    {
    }

    consteval List(std::from_range_t, std::ranges::sized_range auto&& range)
        : _size(std::ranges::size(range))
        , _data{[&]
                {
                    auto refl =
                        substitute(^^create_array, {^^T, std::meta::reflect_constant(_size), ^^decltype(range)});
                    auto fn = std::meta::extract<std::meta::info (*)(decltype(range))>(refl);
                    return fn(std::forward<decltype(range)>(range));
                }()}
    {
    }

    template<typename Range = std::vector<T>>
    consteval Range extract() const
    {
        if (_size == 0)
        {
            return {};
        }

        auto refl = substitute(^^extract_vector, {^^T, ^^Range, std::meta::reflect_constant(_size)});
        auto fn = std::meta::extract<Range (*)(std::meta::info)>(refl);
        return fn(_data);
    }

private:
    template<typename S, size_t n, std::ranges::input_range Range>
    static consteval std::meta::info create_array(Range&& range)
    {
        std::array<S, n> array;
        std::ranges::copy(range, array.begin());
        return std::meta::reflect_constant(array);
    }
    template<typename S, typename Range, size_t n>
    static consteval Range extract_vector(std::meta::info info)
    {
        auto arr = std::meta::extract<std::array<S, n>>(info);
        return {std::from_range, std::move(arr)};
    }
};

struct String
{
    List<char> _characters;

    consteval String() = default;
    template<typename Stringlike>
    consteval String(const Stringlike& str)
        : _characters{std::from_range, str}
    {
    }
    consteval String(const char* str)
        : String{std::string_view{str}}
    {
    }

    consteval std::string extract() const
    {
        return _characters.extract<std::string>();
    }
};

template<typename T>
    requires(is_structural_type(^^T))
struct Optional
{
    std::meta::info _object{};

    consteval Optional() = default;
    consteval Optional(std::nullopt_t) {}

    template<typename U>
    consteval Optional(U&& u)
        : _object{std::meta::reflect_constant(T{std::forward<U>(u)})}
    {
    }

    consteval explicit operator bool() const noexcept
    {
        return _object != std::meta::info{};
    }

    consteval bool has_value() const noexcept
    {
        return _object != std::meta::info{};
    }

    consteval T extract() const
    {
        return extract<T>(_object);
    }
};

} // namespace cxtest::detail::structural