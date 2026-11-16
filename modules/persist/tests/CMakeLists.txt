set(BB_PERSIST_TEST_LIBS ${BLUEBIRD_THREADS_TARGET})

# test_model_sqlite.c talks to the database through sqlite3 directly.
if(BB_WITH_SQLITE)
    list(APPEND BB_PERSIST_TEST_LIBS SQLite::SQLite3)
endif()

bb_add_module_tests(
    persist
    bluebird_persist
    ${BB_PERSIST_TEST_LIBS}
)
