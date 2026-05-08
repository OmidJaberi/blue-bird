add_executable(example_hello
    main.c
    src/app_handlers.c
)

target_include_directories(example_hello
    PRIVATE
        include
)

target_link_libraries(example_hello
    PRIVATE
        bluebird_web
        bluebird_log
)