add_library(bluebird_security)

target_include_directories(bluebird_security
    PUBLIC
        include
)

target_link_libraries(bluebird_security
    PUBLIC
        bluebird_error
)

add_library(BlueBird::security ALIAS bluebird_security)