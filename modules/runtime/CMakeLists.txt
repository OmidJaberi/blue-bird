add_library(bluebird_runtime
    src/task.c
    src/poller/poller.c
    src/scheduler.c
    src/runtime.c
)

# Exactly one poller backend is compiled in, matching the single
# BB_POLLER_BACKEND_* macro selected in blue-bird/utils/platform.h. Keep
# this selection logic in sync with that file -- a mismatch here would
# compile a backend file against a struct bb_poller that doesn't have the
# field it expects (e.g. poller_epoll.c with no BB_POLLER_BACKEND_EPOLL).
if(WIN32)
    target_sources(bluebird_runtime PRIVATE src/poller/poller_poll.c)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(bluebird_runtime PRIVATE src/poller/poller_epoll.c)
elseif(APPLE OR CMAKE_SYSTEM_NAME MATCHES "BSD")
    target_sources(bluebird_runtime PRIVATE src/poller/poller_kqueue.c)
else()
    target_sources(bluebird_runtime PRIVATE src/poller/poller_poll.c)
endif()

target_include_directories(bluebird_runtime
    PUBLIC
        include
    PRIVATE
        internal
)

target_link_libraries(bluebird_runtime
    PUBLIC
        bluebird_error
        bluebird_utils
)

if (WIN32)
    target_link_libraries(bluebird_runtime PRIVATE ws2_32)
endif()

add_library(BlueBird::runtime ALIAS bluebird_runtime)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
