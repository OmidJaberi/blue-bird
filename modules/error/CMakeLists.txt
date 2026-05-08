add_library(bluebird_error
    src/error.c
)

target_include_directories(bluebird_error
    PUBLIC
        include
)

add_library(BlueBird::error ALIAS bluebird_error)