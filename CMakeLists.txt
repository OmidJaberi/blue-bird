cmake_minimum_required(VERSION 3.10)
project(Blue-Bird C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

# Core library
file(GLOB CORE_SOURCES
    "src/core/*.c"
    "src/core/http/*.c"
    "src/persist/*.c"
)

add_library(bluebird_core STATIC ${CORE_SOURCES})
find_package(SQLite3 REQUIRED)
target_link_libraries(bluebird_core PRIVATE SQLite::SQLite3)

# Examples
add_subdirectory(examples)

# Recursively gather test sources
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.c")

foreach(test_file ${TEST_SOURCES})
    # Compute relative path from the tests/ directory
    file(RELATIVE_PATH rel_path "${CMAKE_CURRENT_SOURCE_DIR}/tests" "${test_file}")

    # Extract filename without extension
    get_filename_component(target_name ${rel_path} NAME_WE)

    # Build output path mirroring directory structure
    get_filename_component(rel_dir "${rel_path}" DIRECTORY)
    set(output_dir "${CMAKE_BINARY_DIR}/tests/${rel_dir}")

    # Create the executable
    add_executable(${target_name} ${test_file})
    target_link_libraries(${target_name} bluebird_core)

    # Set the executable output directory
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
    )
endforeach()
