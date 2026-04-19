cmake_minimum_required(VERSION 3.30.0)

project(units LANGUAGES CXX)

add_library(
    units
    INTERFACE
)
target_include_directories(
    units
    INTERFACE
    src
)
target_compile_features(
    units
    INTERFACE
    cxx_std_26
)
target_compile_options(
    units
    INTERFACE
    "-freflection" # Enable reflection in gcc 16.0.1
)

add_subdirectory(../../ cxtest)
add_executable(units_test
    test/dimension.cpp
    test/rational.cpp
    test/unit.cpp
    test/quantity.cpp
)
target_link_libraries(units_test PRIVATE units cxtest_main)
