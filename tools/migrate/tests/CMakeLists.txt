# The SQLite integration tests inspect the database directly, so they link
# SQLite too; they skip themselves in a build with -DBB_WITH_SQLITE=OFF.
# The PostgreSQL test also talks to the server itself (libpq) and runs two
# `up` runs at once (threads); it skips itself unless the build has
# -DBB_WITH_POSTGRES=ON and BB_TEST_POSTGRES_URI points at a database.
set(BB_MIGRATE_TEST_LIBS
    ${BLUEBIRD_THREADS_TARGET}
)

if(BB_WITH_SQLITE)
    list(APPEND BB_MIGRATE_TEST_LIBS SQLite::SQLite3)
endif()

if(BB_WITH_POSTGRES)
    list(APPEND BB_MIGRATE_TEST_LIBS PostgreSQL::PostgreSQL)
endif()

bb_add_module_tests(
    migrate
    bb_migrate_core
    ${BB_MIGRATE_TEST_LIBS}
)
