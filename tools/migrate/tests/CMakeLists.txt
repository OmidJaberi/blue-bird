# Integration tests inspect the database directly, so they link SQLite too.
# The PostgreSQL test also talks to the server itself (libpq) and runs two
# `up` runs at once (threads); it skips itself unless the build has
# -DBB_WITH_POSTGRES=ON and BB_TEST_POSTGRES_URI points at a database.
set(BB_MIGRATE_TEST_LIBS
    SQLite::SQLite3
    ${BLUEBIRD_THREADS_TARGET}
)

if(BB_WITH_POSTGRES)
    list(APPEND BB_MIGRATE_TEST_LIBS PostgreSQL::PostgreSQL)
endif()

bb_add_module_tests(
    migrate
    bb_migrate_core
    ${BB_MIGRATE_TEST_LIBS}
)
