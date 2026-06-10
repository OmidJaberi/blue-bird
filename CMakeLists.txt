cmake_minimum_required(VERSION 3.20)

project(
    BlueBird
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)

# Global warnings
if(MSVC)
    add_compile_options(/W4)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        -Wall
        -Wextra

        # -Wpedantic
        # DSL intentionally uses GNU statement expressions

        # -Werror
    )
elseif(CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic

        # Intentional GNU extensions
        -Wno-gnu-statement-expression-from-macro-expansion
        -Wno-gnu-zero-variadic-macro-arguments

        # -Werror
    )
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Modules
add_subdirectory(modules/error)
add_subdirectory(modules/utils)
add_subdirectory(modules/runtime)
add_subdirectory(modules/security)
add_subdirectory(modules/template)
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
    bluebird_runtime
    bluebird_template
    bluebird_security
)

add_library(BlueBird::bluebird ALIAS bluebird)

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
