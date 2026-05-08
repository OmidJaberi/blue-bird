add_executable(example_todo
    main.c

    src/app_handlers.c
    src/app_middleware.c
    src/app_repo.c
)

target_include_directories(example_todo
    PRIVATE
        include
)

target_link_libraries(example_todo
    PRIVATE
        bluebird_web
        bluebird_persist
        bluebird_log
)