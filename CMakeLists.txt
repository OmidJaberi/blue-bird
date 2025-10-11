cmake_minimum_required(VERSION 3.10)
project(Blue-Bird C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

# Core library
file(GLOB CORE_SOURCES 
    "src/core/*.c"
    "src/core/http/*.c"
)

add_library(bluebird_core STATIC ${CORE_SOURCES})

# Examples
add_subdirectory(examples)

# Unit tests
file(GLOB TEST_SOURCES "tests/*.c")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/unit_tests)
foreach(test_file ${TEST_SOURCES})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} bluebird_core)
    set_target_properties(${test_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
endforeach()
