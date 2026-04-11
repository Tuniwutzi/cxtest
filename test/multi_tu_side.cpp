#include <cxtest/cxtest.hpp>

#include "check.hpp"

namespace
{

namespace multi_tu_side
{

void side_1(cxtest::RTContext& ctx)
{
    ctx.check(!std::is_constant_evaluated());
}
consteval void side_2(cxtest::CTContext& ctx)
{
    ctx.check(std::is_constant_evaluated());
}
constexpr void side_3(cxtest::Context& ctx)
{
    ctx.check(false);
}

} // namespace multi_tu_side

auto registration = cxtest::register_tests_in_namespace<^^multi_tu_side>();

} // namespace
