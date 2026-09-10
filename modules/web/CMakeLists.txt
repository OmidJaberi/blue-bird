add_library(bluebird_web
    src/client.c
    src/server.c
    src/router.c
    src/middleware.c

    src/connection/connection.c
    src/connection/async_connection.c
    src/connection/conn_list.c

    src/http/message.c
    src/http/request.c
    src/http/response.c
    src/http/parser.c

    src/http/client_request.c
    src/http/server_request.c

    src/websocket/websocket.c
    src/websocket/websocket_list.c
    src/websocket/message.c
    src/websocket/frame.c

    src/transport/transport.c
    $<IF:$<BOOL:${BB_WITH_TLS}>,src/transport/tls.c,src/transport/tls_stub.c>
    src/tls_config.c
)

target_include_directories(bluebird_web
    PUBLIC
        include
    PRIVATE
        internal
)
target_link_libraries(bluebird_web
    PUBLIC
        ${BLUEBIRD_THREADS_TARGET}

        bluebird_utils
        bluebird_error
        bluebird_runtime
    PRIVATE
        bluebird_log
)

if(BB_WITH_TLS)
    find_package(OpenSSL REQUIRED)
    # PUBLIC: the test suite (test_tls_context.c, test_https_pipeline.c)
    # talks to OpenSSL directly to drive/verify the TLS transport, and
    # needs both the headers and the link line propagated to it.
    target_link_libraries(bluebird_web PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(bluebird_web PUBLIC BB_WITH_TLS=1)
endif()

add_library(BlueBird::web ALIAS bluebird_web)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
