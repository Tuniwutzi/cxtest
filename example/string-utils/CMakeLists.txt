cmake_minimum_required(VERSION 3.30.0)

project(stringutils LANGUAGES CXX)

add_library(
    stringutils
    INTERFACE
)
target_include_directories(
    stringutils
    INTERFACE
    src
)
target_compile_features(
    stringutils
    INTERFACE
    cxx_std_26
)
target_compile_options(
    stringutils
    INTERFACE
    "-freflection" # Enable reflection in gcc 16.0.1
)

add_subdirectory(../../ cxtest)
add_executable(stringutils_test
    test/length.cpp
    test/name_of.cpp
)
target_link_libraries(stringutils_test PRIVATE stringutils cxtest_main)
