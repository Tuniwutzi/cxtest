#include <cxtest/cxtest.hpp>
#include <units.hpp>

namespace dimension_tests
{

consteval void construction(cxtest::CTContext& ctx)
{
    ctx.check_throws(
        []
        {
            units::Dimension{^^dimension_tests};
        },
        "Base dimension must be a type");
    ctx.check_throws(
        []
        {
            units::Dimension{std::meta::reflect_constant(5)};
        },
        "Base dimension must be a type");
    ctx.check_throws(
        []
        {
            units::Dimension(2);
        });

    ctx.check_nothrow(
        []
        {
            units::Dimension{^^struct Foo};
        },
        "Types are valid base dimensions");
    ctx.check_nothrow(
        []
        {
            units::Dimension(1);
        });
}

consteval void composition(cxtest::CTContext& ctx)
{
    ctx.check(units::Dimension(1) == units::no_dimension);

    units::Dimension a{^^struct A};

    ctx.check(a == a);
    ctx.check(a / a == units::no_dimension);
    ctx.check(a / units::no_dimension == a);
    ctx.check(a * 1 == a);
    ctx.check(a / 1 == a);
    auto a2 = a * a;
    ctx.check(a2 / a == a);
    ctx.check(a2 / a2 == units::no_dimension);
    auto ainv = 1 / a;
    ctx.check(ainv == units::no_dimension / a);
    ctx.check(ainv * a == units::no_dimension);
    ctx.check(ainv * a2 == a);
    ctx.check(ainv / a == 1 / a2);
    ctx.check(a / ainv == a2);

    units::Dimension b{^^struct B};

    ctx.check(a != b);

    ctx.check(a / b == a / b);
    ctx.check(a / b != a && a / b != b);
    {
        auto extracted = units::detail::extract_dimension(a / b);
        ctx.check(extracted.index() == 1);
        auto& factors = std::get<1>(extracted);
        ctx.check(factors.size() == 2);
        ctx.check(std::ranges::contains(factors,
                                        units::detail::DimensionFactor{
                                            .base_dimension = a.value,
                                            .exponent = 1,
                                        }));
        ctx.check(std::ranges::contains(factors,
                                        units::detail::DimensionFactor{
                                            .base_dimension = b.value,
                                            .exponent = -1,
                                        }));
    }
    ctx.check(a / b * b == a);
    ctx.check((a / b) * (b / a) == units::no_dimension);
    ctx.check(1 / (a / b) == b / a);
    ctx.check((a / b) * (1 / (a / b)) == units::no_dimension);
}

consteval void ordering(cxtest::CTContext& ctx)
{
    units::Dimension a{^^struct A};
    units::Dimension b{^^struct B};
    units::Dimension c{^^struct C};

    std::array permutations{
        a * b * c,
        a * c * b,
        b * a * c,
        b * c * a,
        c * a * b,
        c * b * a,
    };
    ctx.check(std::ranges::all_of(permutations,
                                  [&](auto left)
                                  {
                                      return std::ranges::all_of(permutations,
                                                                 [&left](auto right)
                                                                 {
                                                                     return left == right;
                                                                 });
                                  }));
    for (auto remove : std::array{a, b, c})
    {
        ctx.check(std::ranges::all_of(permutations,
                                      [&](auto left)
                                      {
                                          return std::ranges::all_of(permutations,
                                                                     [&](auto right)
                                                                     {
                                                                         return left / remove == right / remove;
                                                                     });
                                      }));
    }
}

} // namespace dimension_tests

static const auto _ = cxtest::register_tests_in_namespace_recursive<^^dimension_tests>();