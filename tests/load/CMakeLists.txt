# Fixed-behaviour server used by the load-test and leak-check CI jobs.
# Not registered with ctest: it runs until signalled.
add_executable(bb-loadtest-server main.c)

target_link_libraries(bb-loadtest-server
    PRIVATE
        bluebird_web
        bluebird_log
)
