#include <jtest/jtest.hpp>

#include "check.hpp"

namespace
{

namespace multi_tu_side
{

void side_1(jtest::RTContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
consteval void side_2(jtest::CTContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void side_3(auto& ctx)
{
    ctx.check(false);
}

} // namespace multi_tu_side

auto registration = jtest::register_tests<^^multi_tu_side>("multi_tu_side");

} // namespace
