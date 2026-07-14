add_executable(example_chat
    main.c

    src/app_handlers.c
    src/app_repo.c
    src/app_ws.c
    src/app_util.c
)

target_include_directories(example_chat
    PRIVATE
        include
)

target_link_libraries(example_chat
    PRIVATE
        bluebird_web
        bluebird_persist
        bluebird_log
        bluebird_security
        bluebird_utils
)

bb_add_assets(
    TARGET example_chat
    DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
)