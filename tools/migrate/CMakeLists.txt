find_package(SQLite3 REQUIRED)

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
    src/drivers/sqlite_driver.c
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

    PRIVATE
        SQLite::SQLite3
)

# PostgreSQL support is optional, like Persist's PostgreSQL backend (it uses
# the same option). Without it, bb-migrate still reads manifests and runs
# against SQLite, and says so if it is pointed at a PostgreSQL database.
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
