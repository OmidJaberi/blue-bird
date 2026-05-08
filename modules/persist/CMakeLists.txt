find_package(SQLite3 REQUIRED)


# Core library
file(GLOB CORE_SOURCES
    src/key_val/*.c
    src/model/*.c
)

add_library(bluebird_persist STATIC ${CORE_SOURCES})

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