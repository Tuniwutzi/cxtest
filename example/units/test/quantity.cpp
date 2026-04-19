#include <cxtest/cxtest.hpp>

#include <units.hpp>

namespace quantity_tests
{

using namespace units;

constexpr void test_constructors(cxtest::Context& ctx)
{
    Meters m{10};
    Kilometers km{10};
    ctx.check(m.count() == 10);
    ctx.check(km.count() == 10);

    // KM -> M is implicit
    ctx.check(std::convertible_to<Kilometers, Meters>);
    Meters m_from_km = km;
    ctx.check(m_from_km.count() == 10000);

    // M -> KM is explicit
    ctx.check(!std::convertible_to<Meters, Kilometers>);
    ctx.check(std::constructible_from<Kilometers, Meters>);
    Kilometers integral_km_from_m{m};
    ctx.check(integral_km_from_m.count() == 0);

    Quantity<kilometers, float> km_from_m{m};
    ctx.check(km_from_m.count() - 0.01f < std::numeric_limits<float>::epsilon());
}

constexpr void test_rep_conversion(cxtest::Context& ctx)
{
    ctx.check(!std::convertible_to<Meters, Meters::Rep>);
    ctx.check(!std::convertible_to<Kilometers, Kilometers::Rep>);
    ctx.check(!std::convertible_to<Quantity<seconds, int>, int>);

    ctx.check(std::convertible_to<Quantity<no_dimension, int>, int>);

    int value = Quantity<no_dimension, int>{1234};
    ctx.check(value == 1234);
}

} // namespace quantity_tests

static const auto _ = cxtest::register_tests_in_namespace_recursive<^^quantity_tests>();
