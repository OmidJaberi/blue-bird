cmake_minimum_required(VERSION 3.20)

project(
    BlueBird
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Global warnings
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Modules
add_subdirectory(modules/error)
add_subdirectory(modules/utils)
add_subdirectory(modules/persist)
add_subdirectory(modules/log)
add_subdirectory(modules/web)


# Umbrella target
add_library(bluebird INTERFACE)

target_link_libraries(bluebird INTERFACE
    bluebird_web
    bluebird_persist
    bluebird_utils
    bluebird_log
    bluebird_error
)

add_library(BlueBird::bluebird ALIAS bluebird)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()