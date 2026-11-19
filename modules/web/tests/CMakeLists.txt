bb_add_module_tests(
    web
    bluebird_web
)

# ---------------------------------------------------------------------------
# Failure-injection tests (fault/)
#
# These compile server.c, async_connection.c, connection.c, conn_list.c and
# http/parser.c a second time, through the wrappers in fault/, with
# malloc/calloc/realloc/bb_strdup and bb_runtime_watch_fd_ex routed through an
# injector (see fault/fault_inject.h). Because the test executable defines
# every symbol of those files itself, the linker never pulls the regular
# copies out of libbluebird_web.a. The injector is wired in per translation
# unit, so it needs no linker or loader support and works the same on every
# toolchain.
# ---------------------------------------------------------------------------
set(BB_WEB_FAULT_SOURCES
    fault/fault_inject.c
    fault/fi_server.c
    fault/fi_async_connection.c
    fault/fi_connection.c
    fault/fi_conn_list.c
    fault/fi_parser.c
)

function(bb_add_web_fault_test test_name)
    set(test_target "web_${test_name}")

    add_executable(${test_target} fault/${test_name}.c ${BB_WEB_FAULT_SOURCES})

    target_link_libraries(${test_target}
        PRIVATE
            bluebird_web
            bluebird_log
    )

    target_include_directories(${test_target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../internal
            ${CMAKE_CURRENT_SOURCE_DIR}/fault
    )

    set_target_properties(${test_target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/fault"
    )

    add_test(NAME "web.${test_name}" COMMAND $<TARGET_FILE:${test_target}>)
endfunction()

# The tests drive the server with raw POSIX sockets.
if(NOT WIN32)
    bb_add_web_fault_test(test_fault_smoke)
    bb_add_web_fault_test(test_fault_server)
    bb_add_web_fault_test(test_fault_async_connection)
endif()
