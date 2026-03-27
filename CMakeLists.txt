cmake_minimum_required(VERSION 3.31.0)

project(jtest)

add_library(jtest STATIC
    src/jtest/jtest.cpp
)
target_compile_features(jtest PUBLIC cxx_std_26)
target_compile_options(jtest PUBLIC
    "-freflection"
)
target_include_directories(jtest PUBLIC src)

add_executable(jtest_tests
    test/main.cpp
    test/execution.cpp
    test/multi_tu_side.cpp
    test/multi_tu_main.cpp
    test/assertions.cpp
)
target_link_libraries(jtest_tests PRIVATE jtest)