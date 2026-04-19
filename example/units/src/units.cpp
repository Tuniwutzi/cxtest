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

struct Dimension
{
    std::meta::info value;
    friend consteval bool operator==(const Dimension&, const Dimension&) noexcept = default;
};

namespace detail
{

using BaseDimension = std::meta::info;
struct DimensionFactor
{
    BaseDimension base_dimension;
    ssize_t factor;
};
template<size_t count>
struct CompositeDimension
{
    std::array<DimensionFactor, count> factors;
};

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
        auto type = type_of(dimension.value);
        auto tmpl = template_of(type);
        if (tmpl != ^^CompositeDimension)
        {
            throw std::meta::exception{"Dimension is not a type nor a composite dimension", dimension.value};
        }

        auto substituted = substitute(^^extract_factors, {std::meta::reflect_constant(dimension.value)});
        auto function = extract<std::span<const DimensionFactor> (*)()>(substituted);
        return function();
    }
}

consteval void sort_dimension_factors(auto& factors)
{
    std::ranges::sort(factors,
                      [](const DimensionFactor& left, const DimensionFactor& right)
                      {
                          return identifier_of(left.base_dimension) <
                                 identifier_of(right.base_dimension); // TODO: proper sorting operator!
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
                pos->factor += factor.factor;
            }
            else
            {
                pos->factor -= factor.factor;
            }
            if (pos->factor == 0)
            {
                factors.erase(pos);
            }
        }
        else
        {
            auto& emplaced = factors.emplace_back(factor);
            if constexpr (!positive)
            {
                emplaced.factor *= -1;
            }
        }
    };

    auto extracted = extract_dimension(dimension);
    auto* extracted_factors = std::get_if<std::span<const DimensionFactor>>(&extracted);
    if (extracted_factors)
    {
        std::ranges::for_each(*extracted_factors, add_factor);
        sort_dimension_factors(factors);
    }
    else
    {
        factors.push_back({std::get<BaseDimension>(extracted), (positive ? 1 : -1)});
    }
}

template<size_t count>
consteval Dimension make_dimension_impl(std::span<const DimensionFactor> factors)
{
    if (factors.size() != count)
    {
        throw std::runtime_error{"make_dimension called with invalid args"};
    }
    CompositeDimension<count> composite{};
    std::ranges::copy(factors, composite.factors.begin());
    return {
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

static constexpr Dimension no_dimension{std::meta::reflect_constant(detail::CompositeDimension<0>{})};

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

struct Ratio
{
    size_t num;
    size_t den;

    friend constexpr bool operator==(const Ratio&, const Ratio&) noexcept = default;

    constexpr void normalize()
    {
        if (den == 0)
        {
            throw std::runtime_error{"Ratio has denominator 0"};
        }

        if (num == 0)
        {
            den = 1;
            return;
        }

        auto gcd = std::gcd(num, den);
        num /= gcd;
        den /= gcd;

        if (den < 0)
        {
            num = -num;
            den = -den;
        }
    }
    friend constexpr Ratio operator/(const Ratio& l, const Ratio& r) noexcept
    {
        Ratio result{
            l.num * r.den,
            l.den * r.num,
        };
        result.normalize();
        return result;
    }
    friend constexpr Ratio operator*(Ratio ratio, size_t factor) noexcept
    {
        ratio.num *= factor;
        ratio.normalize();
        return ratio;
    }
    friend constexpr size_t operator*(size_t factor, Ratio ratio) noexcept
    {
        return (factor * ratio.num) / ratio.den;
    }
};

struct Unit
{
    Dimension dimension;
    Ratio ratio;
    friend constexpr bool operator==(const Unit&, const Unit&) noexcept = default;
};

consteval Unit operator/(const Unit& left, const Unit& right)
{
    auto dimension = left.dimension / right.dimension;
    auto ratio = left.ratio / right.ratio;

    return {
        .dimension = std::move(dimension),
        .ratio = ratio,
    };
}

consteval Unit operator*(const Unit& unit, size_t factor)
{
    return {
        unit.dimension,
        unit.ratio * factor,
    };
}

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
        : _count{other.count() * (other_unit.ratio / unit.ratio)}
    {
    }

    constexpr Rep count() const noexcept
    {
        return _count;
    }

    constexpr operator Rep() const noexcept
        requires(unit.dimension == no_dimension)
    {
        return _count;
    }

    friend constexpr auto operator<=>(const Quantity&, const Quantity&) noexcept = default;

private:
    Rep _count{};
};

template<Unit l, typename L, Unit r, typename R>
constexpr auto operator/(const Quantity<l, L>& left, const Quantity<r, R>& right)
{
    constexpr auto unit = left.unit / right.unit;
    using Rep = std::common_type_t<L, R>;                    // TODO: find proper rep
    Quantity<unit, Rep> value{left.count() / right.count()}; // TODO: cast each to Rep before dividing?
    if constexpr (unit.dimension == no_dimension)
    {
        // if (value.count() != 1)
        // {
        //     throw "1";
        // }
        // static_assert(unit.ratio == Ratio{18, 5});
        return Quantity<{no_dimension, {1, 1}}, Rep>{value};
    }
    else
    {
        return value;
    }
}

constexpr Dimension distance = {^^struct Distance};
constexpr Dimension time = {^^struct Time};
constexpr Unit meters = {distance, Ratio{1, 1}};
constexpr Unit seconds = {distance, Ratio{1, 1}};
using Meters = Quantity<meters, size_t>;
using Seconds = Quantity<seconds, size_t>;

constexpr Unit mps = meters / seconds;
constexpr Unit kmh = (meters * 1000) / (seconds * 3600);
using MpS = Quantity<mps, size_t>;
using KMH = Quantity<kmh, size_t>;

static_assert(mps.dimension == kmh.dimension);
static_assert(mps.ratio == Ratio{1, 1});
static_assert(kmh.ratio == Ratio{5, 18});
static_assert(mps.ratio / kmh.ratio == Ratio{18, 5});
static_assert(KMH{MpS{1000}} == KMH{3600});
static_assert(MpS{KMH{3600}} == MpS{1000});

constexpr auto wtf = MpS{1000} / KMH{3600};
using Wtf = std::remove_cvref_t<decltype(wtf)>;
static_assert(Wtf::unit.dimension == no_dimension);
static_assert(Wtf::unit.ratio == Ratio{1, 1});
static_assert(wtf.count() == 1); // not working due to 1000/3600->0

} // namespace units

int main()
{
    return 0;
}
