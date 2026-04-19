#include <cxtest/cxtest.hpp>

#include <units.hpp>

namespace ratio_tests
{

constexpr void normalization(cxtest::Context& ctx)
{
    units::Rational ratio{4, 8};
    ratio.normalize();
    ctx.check(ratio == units::Rational{1, 2});

    ratio = {5, 3};
    ratio.normalize();
    ctx.check(ratio == units::Rational(5, 3));

    ctx.check_throws(
        []
        {
            units::Rational{1, 0}.normalize();
        });

    ratio = {0, 17};
    ratio.normalize();
    ctx.check(ratio == units::Rational{0, 1});
}

constexpr void division(cxtest::Context& ctx)
{
    using units::Rational;

    units::Rational neutral{1, 1};
    units::Rational half{1, 2};
    units::Rational two{2, 1};

    ctx.check(half / neutral == half);
    ctx.check(two / neutral == two);
    ctx.check(1 / half == two);
    ctx.check(1 / two == half);
    ctx.check(two / half == Rational{4});
    ctx.check(half / two == Rational{1, 4});
}

constexpr void multiplication(cxtest::Context& ctx)
{
    using units::Rational;

    Rational neutral{1, 1};
    Rational half{1, 2};
    Rational two{2, 1};

    ctx.check(half * 1 == half);
    ctx.check(two * 1 == two);
    ctx.check(1 * half == half);
    ctx.check(1 * two == two);
    ctx.check(two * half == Rational{2, 2});
    ctx.check(half * two == Rational{2, 2});
}

constexpr void to_number(cxtest::Context& ctx)
{
    using units::Rational;

    Rational neutral{1, 1};
    Rational half{1, 2};
    Rational two{2, 1};

    ctx.check(static_cast<int>(neutral) == 1);
    ctx.check(static_cast<int>(half) == 0);
    ctx.check(static_cast<int>(two) == 2);

    ctx.check(static_cast<float>(neutral) == 1.f);
    ctx.check(static_cast<float>(half) == 0.5f);
    ctx.check(static_cast<float>(two) == 2.0f);
}

} // namespace ratio_tests

static const auto _ = cxtest::register_tests_in_namespace_recursive<^^ratio_tests>();
