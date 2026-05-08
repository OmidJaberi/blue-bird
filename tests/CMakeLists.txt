file(GLOB_RECURSE TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/integration/*.c"
)

foreach(test_file ${TEST_SOURCES})

    file(RELATIVE_PATH rel_path
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${test_file}"
    )

    get_filename_component(target_name ${rel_path} NAME_WE)
    get_filename_component(rel_dir ${rel_path} DIRECTORY)

    set(output_dir
        "${CMAKE_BINARY_DIR}/tests/${rel_dir}"
    )

    add_executable(${target_name} ${test_file})

    target_link_libraries(${target_name}
        PRIVATE
            bluebird
    )

    set_target_properties(${target_name}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
    )

    add_test(NAME ${target_name}
             COMMAND ${target_name})

endforeach()