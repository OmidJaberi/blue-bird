add_library(bluebird_utils
    src/json.c
    src/time.c
    src/uuid.c
    src/encoding.c
    src/bb_config.c
)

target_include_directories(bluebird_utils
    PUBLIC
        include
)

target_link_libraries(bluebird_utils
    PUBLIC
        bluebird_error
)

add_library(BlueBird::utils ALIAS bluebird_utils)