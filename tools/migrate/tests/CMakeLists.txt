# Integration tests inspect the database directly, so they link SQLite too.
bb_add_module_tests(
    migrate
    bb_migrate_core
    SQLite::SQLite3
)
