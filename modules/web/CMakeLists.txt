add_library(bluebird_web
    src/client.c
    src/server.c
    src/router.c
    src/middleware.c
    src/connection.c
    src/parser.c

    src/http/message.c
    src/http/request.c
    src/http/response.c

    src/http/client_request.c
    src/http/client_response.c

    src/http/server_request.c
    src/http/server_response.c
)

target_include_directories(bluebird_web
    PUBLIC
        include
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