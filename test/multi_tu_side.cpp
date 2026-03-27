#include <jtest/jtest.hpp>

#include "check.hpp"

namespace
{

namespace multi_tu_side
{

void side_1(jtest::RuntimeTestContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
consteval void side_2(jtest::CompiletimeTestContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void side_3(auto& ctx)
{
    ctx.check(false);
}

} // namespace multi_tu_side

jtest::TestGroup<^^multi_tu_side> group{"multi_tu_side"};

} // namespace
