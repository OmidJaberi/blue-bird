add_library(bluebird_log
    src/log.c
    src/console_logger.c
    src/persist_logger.c
)

target_include_directories(bluebird_log
    PUBLIC
        include
)

target_link_libraries(bluebird_log
    PUBLIC
        bluebird_error
    PRIVATE
        bluebird_persist
)

add_library(BlueBird::log ALIAS bluebird_log)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
