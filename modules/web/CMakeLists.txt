add_library(bluebird_web
    src/client.c
    src/server.c
    src/router.c
    src/middleware.c
    src/parser.c

    src/connection/connection.c
    src/connection/async_tasks.c

    src/http/message.c
    src/http/request.c
    src/http/response.c

    src/http/client_request.c
    src/http/server_request.c

    src/websocket/websocket.c
    src/websocket/message.c
    src/websocket/frame.c
    src/websocket/client.c
)

target_include_directories(bluebird_web
    PUBLIC
        include
    PRIVATE
        internal
)

target_link_libraries(bluebird_web
    PUBLIC
        bluebird_utils
        bluebird_error
        bluebird_runtime
    PRIVATE
        bluebird_log
)

add_library(BlueBird::web ALIAS bluebird_web)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
