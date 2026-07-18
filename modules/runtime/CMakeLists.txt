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
    PRIVATE
        bluebird_utils
)

if (WIN32)
    target_link_libraries(bluebird_runtime PRIVATE ws2_32)
endif()

add_library(BlueBird::runtime ALIAS bluebird_runtime)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
