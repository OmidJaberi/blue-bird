find_package(SQLite3 REQUIRED)

# Core is a library (like bb_codegen_core) so tests can link it directly;
# bb-migrate itself is just argument parsing around it.
add_library(bb_migrate_core
    src/migrate_err.c
    src/migration.c
    src/driver.c
    src/plan.c
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
