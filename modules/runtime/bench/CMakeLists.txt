add_executable(bb_runtime_fd_bench fd_lookup_bench.c)

target_link_libraries(bb_runtime_fd_bench PRIVATE bluebird_runtime)

set_target_properties(bb_runtime_fd_bench PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bench"
)
