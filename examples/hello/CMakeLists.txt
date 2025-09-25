file(GLOB EXAMPLE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/main.c"
)

# Create executable
add_executable(hello_example ${EXAMPLE_SOURCES})

# Add include directories (for your headers and core headers)
target_include_directories(hello_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against the core library
target_link_libraries(hello_example PRIVATE bluebird_core)

# Place the built binary in the /examples folder
set_target_properties(hello_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
)