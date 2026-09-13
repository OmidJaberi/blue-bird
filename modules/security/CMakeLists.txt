add_library(bluebird_security
    src/auth.c
    src/config.c
    src/password.c
    src/password_backend.c
    src/pbkdf2.c
    src/random.c
    src/session.c
    src/session_store.c
)

target_include_directories(bluebird_security
    PUBLIC
        include
)

target_link_libraries(bluebird_security
    PUBLIC
        bluebird_error
        # bb_security_config_from_json() takes a bb_json_t (blue-bird/security/config.h),
        # so bluebird_utils's headers need to reach anyone consuming
        # this module's public API, not just this module's own .c files.
        bluebird_utils
)

add_library(BlueBird::security ALIAS bluebird_security)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
