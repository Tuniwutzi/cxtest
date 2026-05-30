#include <cxtest/cxtest.hpp>

#include "check.hpp"

namespace[[= cxtest::templated(^^int, ^^float)]] g1
{

template<typename T>
constexpr void t1(cxtest::Context& ctx)
{
    ctx.check(std::same_as<T, int> || std::same_as<T, float>);
    ctx.check(false, std::same_as<T, int> ? "int" : std::same_as<T, float> ? "float" : "");
}

} // namespace g1

namespace g2
{

struct Foo
{
    struct Bar
    {
        int i;
    } bar;
};

} // namespace g2

namespace[[= cxtest::templated(g2::Foo{1}, 2)]] g2
{

template<auto i>
constexpr void t(cxtest::Context& ctx)
{
    if constexpr (^^decltype(i) == ^^Foo)
    {
        ctx.check(i.bar.i == 1);
        ctx.check(false, i.bar.i == 1 ? "Foo{1}" : "");
    }
    else
    {
        ctx.check(i == 2);
        ctx.check(false, i == 2 ? "2" : "");
    }
}

} // namespace g2

namespace[[= cxtest::templated(^^int)]] g3
{

template<typename T>
constexpr void t1(cxtest::Context&)
{
}
template<typename T>
constexpr void t2(cxtest::Context&)
{
}

} // namespace g3

namespace[[= cxtest::templated(^^int)]] g4
{

namespace[[= cxtest::templated(^^float)]] g4a
{

template<typename T>
constexpr void t(cxtest::Context&)
{
}

} // namespace g4a

} // namespace g4

namespace
{

void check_test(auto&& tests, std::string test_name, std::string_view expectation)
{
    auto check_execution = [&](auto& execution)
    {
        REQUIRE(execution.has_value(), "Expected CT and RT execution");
        REQUIRE(execution->failures.size() == 1, "Expected exactly one failure");
        REQUIRE(execution->failures.front() == expectation,
                std::format("Unexpected failure message: {} != {}", execution->failures.front(), expectation));
    };

    auto test_pos = tests.find(test_name);
    REQUIRE(test_pos != tests.end(), std::format("Did not find expected test {}", test_name));
    auto& test = test_pos->second;
    check_execution(test.ct_sink);
    check_execution(test.rt_sink);
};

void test_g1()
{
    auto groups = cxtest::detail::discover_groups_recursive<^^g1>();
    REQUIRE(groups.size() == 1, "Expected to discover 1 group from namespace ::g1");
    REQUIRE(groups.front().get_name() == "g1", "Unexpected name for group from ::g1");

    auto& tests = groups.front().get_tests();
    REQUIRE(tests.size() == 2, "Unexpected test count in ::g1");
    REQUIRE(tests[0].name == std::string_view{"t1<int>"}, std::format("Unexpected test {} in ::g1", tests[0].name));
    REQUIRE(tests[1].name == std::string_view{"t1<float>"}, "Unexpected test in ::g1");

    cxtest::CollectingGroupOutputSink sink{};
    groups.front().run(sink);
    REQUIRE(sink.tests.size() == 2, "Unexpected test count");

    check_test(sink.tests, "t1<int>", "int");
    check_test(sink.tests, "t1<float>", "float");
}

void test_g2()
{
    auto groups = cxtest::detail::discover_groups_recursive<^^g2>();
    REQUIRE(groups.size() == 1, "Expected to discover 1 group from namespace ::g2");
    REQUIRE(groups.front().get_name() == "g2", "Unexpected name for group from ::g2");

    auto& tests = groups.front().get_tests();
    REQUIRE(tests.size() == 2, "Unexpected test count in ::g2");
    REQUIRE(tests[0].name == std::string_view{"t<g2::Foo{g2::Foo::Bar{1}}>"},
            std::format("Unexpected test {} in ::g2", tests[0].name));
    REQUIRE(tests[1].name == std::string_view{"t<2>"}, "Unexpected test in ::g2");

    cxtest::CollectingGroupOutputSink sink{};
    groups.front().run(sink);
    REQUIRE(sink.tests.size() == 2, "Unexpected test count");

    check_test(sink.tests, "t<g2::Foo{g2::Foo::Bar{1}}>", "Foo{1}");
    check_test(sink.tests, "t<2>", "2");
}

void test_g3()
{
    auto groups = cxtest::detail::discover_groups_recursive<^^g3>();
    REQUIRE(groups.size() == 1, "Expected to discover 1 group from namespace ::g3");
    REQUIRE(groups.front().get_name() == "g3", "Unexpected name for group from ::g3");

    auto& tests = groups.front().get_tests();
    REQUIRE(tests.size() == 2, "Unexpected test count");
    REQUIRE(tests[0].name == std::string_view{"t1<int>"}, std::format("Unexpected test name: {}", tests[0].name));
    REQUIRE(tests[1].name == std::string_view{"t2<int>"}, "Unexpected test name");
}

void test_g4()
{
    bool threw = [] consteval -> bool
    {
        try
        {
            cxtest::detail::get_template_values(^^g4::g4a);
            return false;
        }
        catch (const std::exception& ex)
        {
            return true;
        }
    }();

    REQUIRE(threw, "Should have thrown because of ambiguous templated annotation");
}

} // namespace

void test_templated()
{
    test_g1();
    test_g2();
    test_g3();
    test_g4();
}
