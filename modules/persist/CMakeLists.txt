add_library(bluebird_persist
    src/key_val/persist_file.c
    src/key_val/persist_json.c
    src/key_val/persist_registry.c

    src/model/model_json.c
    src/model/model_registry.c
    src/model/query.c
    src/model/repo.c
    src/model/schema.c

    src/serialization/entity_json.c
)

# SQLite is optional, like PostgreSQL below (-DBB_WITH_SQLITE=OFF). Without it
# the "sqlite" key/value and model backends are not compiled, so
# bb_persist_kv_sqlite_register() and bb_model_sqlite_api() are not available;
# BB_WITH_SQLITE is defined for code that links this module so it can guard
# calls to them.
if(BB_WITH_SQLITE)
    find_package(SQLite3 REQUIRED)
    target_sources(bluebird_persist PRIVATE
        src/key_val/persist_sqlite.c
        src/model/model_sqlite.c
    )
    target_link_libraries(bluebird_persist PRIVATE SQLite::SQLite3)
    target_compile_definitions(bluebird_persist PUBLIC BB_WITH_SQLITE=1)
endif()

if(BB_WITH_POSTGRES)
    find_package(PostgreSQL REQUIRED)
    target_sources(bluebird_persist PRIVATE src/model/model_postgres.c)
    target_link_libraries(bluebird_persist PRIVATE PostgreSQL::PostgreSQL)
    target_compile_definitions(bluebird_persist PUBLIC BB_WITH_POSTGRES=1)
endif()

target_include_directories(bluebird_persist
    PUBLIC
        include
)

target_link_libraries(bluebird_persist
    PUBLIC
        bluebird_utils
        bluebird_error

    PRIVATE
        ${BLUEBIRD_THREADS_TARGET}
)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
