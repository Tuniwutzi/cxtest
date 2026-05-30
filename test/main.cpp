#include <algorithm>
#include <array>
#include <iostream>
#include <meta>
#include <string>

void test_execution();
void test_multi_tu();
void test_assertions();
void test_groups();
void test_templated();

namespace
{
template<std::meta::info info>
struct Test
{
    std::string name{identifier_of(info)};
    void (*function)() = [:info:];
};

bool execute_test(auto& test)
{
    std::cout << "Test " << test.name;
    try
    {
        test.function();
        std::cout << " succeeded\n";
        return true;
    }
    catch (const std::exception& ex)
    {
        std::cout << " failed with error: " << ex.what() << "\n";
        return false;
    }
}

template<typename Tuple>
bool execute(const Tuple& tuple)
{
    auto& [... tests] = tuple;
    return ((execute_test(tests)) & ...);
}
} // namespace

int main()
{
    std::tuple tests{
        Test<^^test_execution>{},
        Test<^^test_multi_tu>{},
        Test<^^test_assertions>{},
        Test<^^test_groups>(),
        Test<^^test_templated>(),
    };
    return execute(tests) ? 0 : 1;
}
