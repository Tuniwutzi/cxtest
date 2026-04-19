#include <cxtest/cxtest.hpp>

#include <units.hpp>

namespace unit_tests
{

using units::Dimension;
using units::Rational;
using units::Unit;

consteval void operators(cxtest::CTContext& ctx)
{
    Dimension da{^^struct A};
    Dimension db{^^struct B};

    Unit a{da, {1}};
    Unit b{db, {1}};

    auto ka = 1000 * a;
    ctx.check(ka.dimension == da && ka.ratio == Rational{1000, 1});
    ctx.check((ka / 50).ratio == Rational{20, 1});

    auto ca = 100 * a;
    ctx.check(ca.dimension == da && ca.ratio == Rational{100, 1});
    ctx.check(ka / 10 == ca);

    auto ma = a / 1000;
    ctx.check(ma.dimension == da && ma.ratio == Rational{1, 1000});

    auto one = ma / ma;
    ctx.check(one.dimension == Dimension{1} && one.ratio == Rational{1});

    auto kone = ka / a;
    ctx.check(kone.dimension == Dimension{1} && kone.ratio == Rational{1000});

    auto a_by_b = ka / (b / 1000);
    ctx.check(a_by_b.dimension == da / db && a_by_b.ratio == 1000000);
    auto a_times_b = ma * (1000 * b);
    ctx.check(a_times_b.dimension == da * db && a_times_b.ratio == Rational{1});
}

} // namespace unit_tests

static const auto _ = cxtest::register_tests_in_namespace_recursive<^^unit_tests>();
