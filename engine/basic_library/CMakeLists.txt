cmake_minimum_required(VERSION 3.21)

project(basic_library LANGUAGES CXX)

add_library(basic_library
    src/basic_library.cpp
    src/loop_runner_plugin.cpp
)

target_include_directories(basic_library
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(basic_library PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(basic_library PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(basic_library PRIVATE -Wall -Wextra -Wpedantic)
endif()
