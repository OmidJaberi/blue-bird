function(bb_add_module_tests MODULE_NAME MODULE_TARGET)

    file(GLOB_RECURSE TEST_SOURCES
        CONFIGURE_DEPENDS
        unit/*.c
        integration/*.c
    )

    foreach(test_file ${TEST_SOURCES})

        get_filename_component(test_name
            ${test_file}
            NAME_WE
        )

        file(RELATIVE_PATH rel_path
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${test_file}
        )

        if(rel_path MATCHES "^unit/")
            set(test_category "unit")
        elseif(rel_path MATCHES "^integration/")
            set(test_category "integration")
        else()
            message(FATAL_ERROR
                "Unknown test category: ${test_file}"
            )
        endif()

        set(test_target
            "${MODULE_NAME}_${test_name}"
        )

        add_executable(
            ${test_target}
            ${test_file}
        )

        target_link_libraries(
            ${test_target}
            PRIVATE
                ${MODULE_TARGET}
        )

        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../internal")
            target_include_directories(
                ${test_target}
                PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/../internal
            )
        endif()

        set_target_properties(
            ${test_target}
            PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY
                "${CMAKE_BINARY_DIR}/tests/${test_category}"
        )

        add_test(
            NAME "${MODULE_NAME}.${test_name}"
            COMMAND $<TARGET_FILE:${test_target}>
        )

    endforeach()

endfunction()
