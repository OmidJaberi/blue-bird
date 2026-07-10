add_library(bluebird_runtime
    src/task.c
    src/poller.c
    src/scheduler.c
    src/runtime.c
)

target_include_directories(bluebird_runtime
    PUBLIC
        include
    PRIVATE
        internal
)

target_link_libraries(bluebird_runtime
    PUBLIC
        bluebird_error
)

add_library(BlueBird::runtime ALIAS bluebird_runtime)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
