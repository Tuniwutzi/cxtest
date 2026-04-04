add_library(stringutils INTERFACE)
target_include_directories(stringutils INTERFACE
    src
)

add_executable(stringutils_test
    test/length.cpp
    test/name_of.cpp
)
target_link_libraries(stringutils_test PRIVATE stringutils cxtest_main)