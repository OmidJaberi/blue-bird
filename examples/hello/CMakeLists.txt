file(GLOB EXAMPLE_SOURCES "*.c")

add_executable(hello_example ${EXAMPLE_SOURCES})
target_link_libraries(hello_example bluebird_core)

set_target_properties(hello_example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples)