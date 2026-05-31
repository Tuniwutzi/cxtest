#pragma once

#include <algorithm>
#include <array>
#include <meta>
#include <ranges>
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

    consteval std::vector<T> extract() const
    {
        if (_size == 0)
        {
            return {};
        }

        auto refl = substitute(^^extract_vector, {^^T, std::meta::reflect_constant(_size)});
        auto fn = std::meta::extract<std::vector<T> (*)(std::meta::info)>(refl);
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
    template<typename S, size_t n>
    static consteval std::vector<S> extract_vector(std::meta::info info)
    {
        auto arr = std::meta::extract<std::array<S, n>>(info);
        return {std::from_range, std::move(arr)};
    }
};

} // namespace cxtest::detail::structural