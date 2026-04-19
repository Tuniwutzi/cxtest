cmake_minimum_required(VERSION 3.30.0)

project(units LANGUAGES CXX)

add_executable(
    units
    src/units.cpp
)
target_include_directories(
    units
    PRIVATE
    src
)
target_compile_features(
    units
    PRIVATE
    cxx_std_26
)

target_compile_options(
    units
    PRIVATE
    "-freflection" # Enable reflection in gcc 16.0.1
)
