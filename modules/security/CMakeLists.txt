add_library(bluebird_security
    src/auth.c
    src/password.c
    src/random.c
    src/session.c
)

target_include_directories(bluebird_security
    PUBLIC
        include
)

target_link_libraries(bluebird_security
    PUBLIC
        bluebird_error
)

add_library(BlueBird::security ALIAS bluebird_security)