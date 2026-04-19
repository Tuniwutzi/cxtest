#include <algorithm>
#include <array>
#include <meta>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace units
{

namespace detail
{

using BaseDimension = std::meta::info;
struct DimensionFactor
{
    BaseDimension base_dimension;
    ssize_t exponent;
    friend consteval bool operator==(const DimensionFactor&, const DimensionFactor&) noexcept = default;
};
template<size_t count>
struct CompositeDimension
{
    std::array<DimensionFactor, count> factors;
};

} // namespace detail

struct Dimension
{
    const std::meta::info value;
    consteval explicit Dimension(std::meta::info value)
        : value{value}
    {
        if (!is_type(value))
        {
            auto type = type_of(value);
            auto tmpl = template_of(type);
            if (tmpl != ^^detail::CompositeDimension)
            {
                throw std::meta::exception{"Dimension must be a type or a composite dimension", value};
            }
        }
    }
    consteval Dimension(size_t one)
        : Dimension{std::meta::reflect_constant(detail::CompositeDimension<0>{})}
    {
        if (one != 1)
        {
            throw std::runtime_error{"Empty dimension must be constructed from 1"};
        }
    }

    friend consteval bool operator==(const Dimension&, const Dimension&) noexcept = default;
};

static constexpr Dimension no_dimension{1};

namespace detail
{

template<std::meta::info composite_dimension>
consteval std::span<const DimensionFactor> extract_factors()
{
    return [:composite_dimension:].factors;
}

consteval std::variant<BaseDimension, std::span<const DimensionFactor>> extract_dimension(const Dimension& dimension)
{
    if (is_type(dimension.value))
    {
        return dimension.value;
    }
    else
    {
        auto substituted = substitute(^^extract_factors, {std::meta::reflect_constant(dimension.value)});
        auto function = extract<std::span<const DimensionFactor> (*)()>(substituted);
        return function();
    }
}

template<typename L, typename R>
constexpr bool less_than = std::type_order_v<L, R> < 0;

consteval void sort_dimension_factors(auto& factors)
{
    std::ranges::sort(factors,
                      [](const DimensionFactor& left, const DimensionFactor& right)
                      {
                          return extract<bool>(substitute(^^less_than, {left.base_dimension, right.base_dimension}));
                      });
}

template<bool positive>
consteval void add_factors(std::vector<DimensionFactor>& factors, const Dimension& dimension)
{
    auto add_factor = [&factors](const DimensionFactor& factor)
    {
        auto pos = std::ranges::find(factors, factor.base_dimension, &DimensionFactor::base_dimension);
        if (pos != factors.end())
        {
            if constexpr (positive)
            {
                pos->exponent += factor.exponent;
            }
            else
            {
                pos->exponent -= factor.exponent;
            }
            if (pos->exponent == 0)
            {
                factors.erase(pos);
            }
        }
        else
        {
            auto& emplaced = factors.emplace_back(factor);
            if constexpr (!positive)
            {
                emplaced.exponent *= -1;
            }
        }
    };

    auto extracted = extract_dimension(dimension);
    auto* extracted_factors = std::get_if<std::span<const DimensionFactor>>(&extracted);
    if (extracted_factors)
    {
        std::ranges::for_each(*extracted_factors, add_factor);
    }
    else
    {
        DimensionFactor factor{std::get<BaseDimension>(extracted), 1};
        add_factor(factor);
    }
}

template<size_t count>
consteval Dimension make_dimension_impl(std::span<const DimensionFactor> factors)
{
    if (factors.size() != count)
    {
        throw std::runtime_error{"make_dimension called with invalid args"};
    }
    if constexpr (count == 1)
    {
        if (factors.front().exponent == 1)
        {
            return Dimension{factors.front().base_dimension};
        }
    }
    CompositeDimension<count> composite{};
    std::ranges::copy(factors, composite.factors.begin());
    sort_dimension_factors(composite.factors);
    return Dimension{
        std::meta::reflect_constant(composite),
    };
}

consteval Dimension make_dimension(std::span<const DimensionFactor> factors)
{
    size_t count = factors.size();
    auto impl = substitute(^^make_dimension_impl, {std::meta::reflect_constant(count)});
    auto function = extract<Dimension (*)(std::span<const DimensionFactor>)>(impl);
    return function(factors);
}

} // namespace detail

consteval Dimension operator/(const Dimension& left, const Dimension& right)
{
    std::vector<detail::DimensionFactor> factors{};
    detail::add_factors<true>(factors, left);
    detail::add_factors<false>(factors, right);
    return detail::make_dimension(factors);
}

consteval Dimension operator*(const Dimension& left, const Dimension& right)
{
    std::vector<detail::DimensionFactor> factors{};
    detail::add_factors<true>(factors, left);
    detail::add_factors<true>(factors, right);
    return detail::make_dimension(factors);
}

struct Rational
{
    using Scalar = size_t;
    Scalar num;
    Scalar den;

    constexpr Rational(Scalar num, Scalar den = 1)
        : num{num}
        , den{den}
    {
        if (den == 0)
        {
            throw std::out_of_range{"Denominator must not be 0"};
        }
    }

    friend constexpr bool operator==(const Rational&, const Rational&) noexcept = default;

    constexpr void normalize()
    {
        if (den == 0)
        {
            throw std::runtime_error{"Rational has denominator 0"};
        }

        if (num == 0)
        {
            den = 1;
            return;
        }

        auto gcd = std::gcd(num, den);
        num /= gcd;
        den /= gcd;

        static_assert(!std::is_signed_v<Scalar>,
                      "Implementation assumes unsigned denominator - otherwise we must make sure it's positive after "
                      "normalization");
    }

    template<typename T>
        requires(std::is_arithmetic_v<T> && !std::same_as<bool, T>)
    constexpr explicit operator T() const noexcept
    {
        return static_cast<T>(num) / den;
    }

    friend constexpr Rational operator/(const Rational& l, const Rational& r) noexcept
    {
        Rational result{
            l.num * r.den,
            l.den * r.num,
        };
        return result;
    }
    friend constexpr Rational operator*(const Rational& l, const Rational& r) noexcept
    {
        Rational result{
            l.num * r.num,
            l.den * r.den,
        };
        return result;
    }
};

struct Unit
{
    Dimension dimension;
    Rational ratio;
    consteval Unit(Dimension d, Rational r = 1)
        : dimension{d}
        , ratio{r}
    {
        ratio.normalize();
    }
    consteval Unit(Rational r)
        : Unit{no_dimension, r}
    {
    }
    consteval Unit(size_t factor)
        : Unit{Rational{factor}}
    {
    }
    friend consteval bool operator==(const Unit&, const Unit&) noexcept = default;

    friend consteval Unit operator/(const Unit& left, const Unit& right)
    {
        auto dimension = left.dimension / right.dimension;
        auto ratio = left.ratio / right.ratio;

        return {
            std::move(dimension),
            ratio,
        };
    }

    friend consteval Unit operator*(const Unit& left, const Unit& right)
    {
        return {
            left.dimension * right.dimension,
            left.ratio * right.ratio,
        };
    }
};

/**
 * A quantity represents a unit with an actual value representation. This is the class that will actually be used in
 * real code at runtime.
 * @warning This class is a minimal proof-of-concept. It doesn't handle conversions correctly in terms of overflow or
 * any other kind of edge case.
 */
template<Unit unit_arg, typename RepArg>
struct Quantity
{
    static constexpr Unit unit = unit_arg;
    using Rep = RepArg;

    constexpr explicit Quantity(Rep value)
        : _count{value}
    {
    }

    template<Unit other_unit, typename OtherRep>
        requires(other_unit.dimension == unit.dimension)
    constexpr explicit((other_unit.ratio / unit.ratio).den != 1) Quantity(const Quantity<other_unit, OtherRep>& other)
        : _count{static_cast<Rep>(other.count()) * [] consteval
                 {
                     // Weird hack.
                     // Needed because dividing the ratios inline (other.count() * (other_unit.ratio / unit.ratio))
                     // causes errors when used at compile time with non-constexpr "other".
                     return static_cast<Rep>(other_unit.ratio) / static_cast<Rep>(unit.ratio);
                 }()}
    {
    }

    constexpr Rep count() const noexcept
    {
        return _count;
    }

    constexpr operator Rep() const noexcept
        requires(unit.dimension == no_dimension && unit.ratio == 1)
    {
        return _count;
    }

private:
    Rep _count{};
};

constexpr Dimension distance{^^struct Distance};
constexpr Unit meters{distance};
constexpr Unit kilometers = meters * 1000;
constexpr Unit millimeters = meters / 1000;
using Meters = Quantity<meters, size_t>;
using Kilometers = Quantity<kilometers, size_t>;
using Millimeters = Quantity<millimeters, size_t>;

constexpr Dimension time{^^struct Time};
constexpr Unit seconds{time};
constexpr Unit hours = seconds * 3600;
constexpr Unit milliseconds = seconds / 3600;
using Seconds = Quantity<seconds, size_t>;
using Hours = Quantity<hours, size_t>;
using Milliseconds = Quantity<millimeters, size_t>;

using MpS = Quantity<meters / seconds, size_t>;
using KMpH = Quantity<kilometers / hours, size_t>;

} // namespace units
