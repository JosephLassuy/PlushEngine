cmake_minimum_required(VERSION 3.21)

project(PlushEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not supported. Please configure the build in a separate directory, e.g. `cmake -S . -B build`.")
endif()

add_subdirectory(engine/basic_library)

add_executable(PlushEngine
    src/main.cpp
)

target_link_libraries(PlushEngine PRIVATE basic_library)

target_include_directories(PlushEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(PlushEngine PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
else()
    target_compile_options(PlushEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Examples
add_executable(Flappybird example/flappybird/main.cpp)
target_link_libraries(Flappybird PRIVATE basic_library)
target_include_directories(Flappybird PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(Flappybird PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
else()
    target_compile_options(Flappybird PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(Launcher example/launcher/main.cpp)
target_link_libraries(Launcher PRIVATE basic_library)
target_include_directories(Launcher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
    target_compile_options(Launcher PRIVATE /W4 /permissive- /Zc:preprocessor /EHsc)
else()
    target_compile_options(Launcher PRIVATE -Wall -Wextra -Wpedantic)
endif()
