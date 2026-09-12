add_library(bluebird_security
    src/auth.c
    src/password.c
    src/password_backend.c
    src/pbkdf2.c
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
    PRIVATE
        bluebird_utils
)

add_library(BlueBird::security ALIAS bluebird_security)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
