cmake_minimum_required(VERSION 3.31)

project(String-Utils)

add_library(stringutils INTERFACE)
target_include_directories(stringutils INTERFACE
    src
)

add_subdirectory(../../ jtest)

add_executable(stringutils_test
    test/length.cpp
    test/name_of.cpp
)
target_link_libraries(stringutils_test PRIVATE stringutils jtest_main)