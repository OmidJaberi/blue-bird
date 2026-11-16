# The chat example stores its messages in SQLite, so it is only built when
# the SQLite backend is (-DBB_WITH_SQLITE=OFF skips it).
if(NOT BB_WITH_SQLITE)
    message(STATUS "Skipping example_chat (needs BB_WITH_SQLITE)")
    return()
endif()

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
        bluebird
)

bb_add_assets(
    TARGET example_chat
    DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
)