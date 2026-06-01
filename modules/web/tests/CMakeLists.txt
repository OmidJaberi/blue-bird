file(GLOB_RECURSE WEB_TEST_SOURCES
    CONFIGURE_DEPENDS
    "*.c"
)

foreach(test_file ${WEB_TEST_SOURCES})

    get_filename_component(test_name
        ${test_file}
        NAME_WE
    )

    # Unique target name
    set(test_target "web_${test_name}")

    add_executable(${test_target}
        ${test_file}
    )

    target_link_libraries(${test_target}
        PRIVATE
            bluebird_web
    )

    # Allow access to internal headers
    target_include_directories(${test_target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../internal
    )

    # Put binaries in build/tests/web
    set_target_properties(${test_target}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/tests/web"
    )

    # Register with CTest using the actual executable path
    add_test(
        NAME web.${test_name}
        COMMAND $<TARGET_FILE:${test_target}>
    )

endforeach()
