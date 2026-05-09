find_package(SQLite3 REQUIRED)

add_library(bluebird_persist
    src/key_val/persist_file.c
    src/key_val/persist_json.c
    src/key_val/persist_registry.c
    src/key_val/persist_sqlite.c

    src/model/model_json.c
    src/model/model_registry.c
    src/model/model_sqlite.c
    src/model/repo.c
    src/model/schema.c

    src/serialization/entity_json.c
)

target_include_directories(bluebird_persist
    PUBLIC
        include
)

target_link_libraries(bluebird_persist
    PUBLIC
        bluebird_utils
        bluebird_error

    PRIVATE
        SQLite::SQLite3
)