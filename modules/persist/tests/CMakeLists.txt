file(GLOB_RECURSE PERSIST_TEST_SOURCES
    CONFIGURE_DEPENDS
    unit/*.c
    integration/*.c
)

foreach(test_file ${PERSIST_TEST_SOURCES})

    get_filename_component(test_name
        ${test_file}
        NAME_WE
    )

    file(RELATIVE_PATH rel_path
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${test_file}
    )

    # Determine category from path
    if(rel_path MATCHES "^unit/")
        set(test_category "unit")
    elseif(rel_path MATCHES "^integration/")
        set(test_category "integration")
    else()
        message(FATAL_ERROR
            "Unknown persist test category for ${test_file}"
        )
    endif()

    # Unique target name
    set(test_target "persist_${test_name}")

    add_executable(${test_target}
        ${test_file}
    )

    target_link_libraries(${test_target}
        PRIVATE
            bluebird_persist
    )

    # Allow access to internal headers
    target_include_directories(${test_target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../internal
    )

    # Output:
    # build/tests/unit/
    # build/tests/integration/
    set_target_properties(${test_target}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY
            "${CMAKE_BINARY_DIR}/tests/${test_category}"
    )

    add_test(
        NAME persist.${test_name}
        COMMAND $<TARGET_FILE:${test_target}>
    )

endforeach()
