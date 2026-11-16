# Core is a library (like bb_codegen_core) so tests can link it directly;
# bb-migrate itself is just argument parsing around it.
add_library(bb_migrate_core
    src/migrate_err.c
    src/migration.c
    src/driver.c
    src/plan.c
    src/table_spec.c
    src/generate.c
    src/runner.c
)

target_include_directories(bb_migrate_core
    PUBLIC
        src
)

# Deliberately does NOT link bluebird_persist: migrations need their own
# single transactional connection (see src/driver.h).
target_link_libraries(bb_migrate_core
    PUBLIC
        bluebird_error
        bluebird_utils
)

# Both database drivers are optional and use the same options as Persist's
# backends. Without SQLite (-DBB_WITH_SQLITE=OFF) bb-migrate says so if it is
# pointed at a SQLite file; without PostgreSQL it says so if it is pointed at
# a PostgreSQL database. It still reads manifests either way, and a build
# with neither driver can only report that.
if(BB_WITH_SQLITE)
    find_package(SQLite3 REQUIRED)

    target_sources(bb_migrate_core PRIVATE src/drivers/sqlite_driver.c)
    target_link_libraries(bb_migrate_core PRIVATE SQLite::SQLite3)
    target_compile_definitions(bb_migrate_core PUBLIC BB_WITH_SQLITE=1)
endif()

if(BB_WITH_POSTGRES)
    find_package(PostgreSQL REQUIRED)

    target_sources(bb_migrate_core PRIVATE src/drivers/postgres_driver.c)
    target_link_libraries(bb_migrate_core PRIVATE PostgreSQL::PostgreSQL)
    target_compile_definitions(bb_migrate_core PUBLIC BB_WITH_POSTGRES=1)
endif()

add_executable(bb-migrate
    src/main.c
)

target_link_libraries(bb-migrate
    PRIVATE
        bb_migrate_core
)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
