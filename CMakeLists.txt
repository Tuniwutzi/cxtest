cmake_minimum_required(VERSION 3.30.0)

project(cxtest)

option(CXTEST_BUILD_TESTS "Whether to include tests for cxtest" ${PROJECT_IS_TOP_LEVEL})

add_library(cxtest STATIC
    src/cxtest/cxtest.cpp
)
target_compile_features(cxtest PUBLIC cxx_std_26)
# GCC trunk (16.0.1 at time of writing) needs -freflection
# Future compilers will likely support reflection out of the box
# Add the flag if supported, but try compiling in any case
include(CheckCXXCompilerFlag)
set(CMAKE_REQUIRED_FLAGS "-std=c++26")
check_cxx_compiler_flag("-freflection" HAS_FREFLECTION)
if (HAS_FREFLECTION)
    target_compile_options(cxtest PUBLIC -freflection)
endif()
target_include_directories(cxtest PUBLIC src)
target_compile_definitions(cxtest PUBLIC CXTEST_VERSION_FILE="${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

add_library(cxtest_main STATIC
    src/cxtest/cxtest_main.cpp
)
target_link_libraries(cxtest_main PUBLIC cxtest)

if (CXTEST_BUILD_TESTS)
add_executable(cxtest_tests
    test/main.cpp
    test/execution.cpp
    # test/multi_tu_side.cpp
    # test/multi_tu_main.cpp
    test/assertions.cpp
    # test/groups.cpp
)
target_link_libraries(cxtest_tests PRIVATE cxtest)
endif()
