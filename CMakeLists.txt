cmake_minimum_required(VERSION 3.31.0)

project(cxtest)

option(CXTEST_BUILD_TESTS "Whether to include tests for cxtest" ${PROJECT_IS_TOP_LEVEL})
option(CXTEST_BUILD_EXAMPLES "Whether to include examples for cxtest" ${PROJECT_IS_TOP_LEVEL})

add_library(cxtest STATIC
    src/cxtest/cxtest.cpp
)
target_compile_features(cxtest PUBLIC cxx_std_26)
target_compile_options(cxtest PUBLIC
    "-freflection"
)
target_include_directories(cxtest PUBLIC src)

add_library(cxtest_main STATIC
    src/cxtest/cxtest_main.cpp
)
target_link_libraries(cxtest_main PUBLIC cxtest)

if (CXTEST_BUILD_TESTS)
add_executable(cxtest_tests
    test/main.cpp
    test/execution.cpp
    test/multi_tu_side.cpp
    test/multi_tu_main.cpp
    test/assertions.cpp
)
target_link_libraries(cxtest_tests PRIVATE cxtest)
endif()

if (CXTEST_BUILD_EXAMPLES)
add_subdirectory(example/string-utils)
endif()
