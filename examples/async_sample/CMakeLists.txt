add_executable(example_async
    main.c
)

target_include_directories(example_async
    PRIVATE
        include
)

target_link_libraries(example_async
    PRIVATE
        bluebird_runtime
)