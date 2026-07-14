include(CMakeParseArguments)

function(bb_add_assets)
    set(options)
    set(oneValueArgs TARGET DESTINATION)
    set(multiValueArgs DIRECTORIES FILES)

    cmake_parse_arguments(BB
        "${options}"
        "${oneValueArgs}"
        "${multiValueArgs}"
        ${ARGN}
    )

    if(NOT BB_TARGET)
        message(FATAL_ERROR "bb_add_assets: TARGET is required")
    endif()

    if(NOT BB_DESTINATION)
        set(BB_DESTINATION ".")
    endif()

    foreach(dir IN LISTS BB_DIRECTORIES)
        get_filename_component(name "${dir}" NAME)

        add_custom_command(
            TARGET ${BB_TARGET}
            POST_BUILD
            COMMAND
                ${CMAKE_COMMAND} -E copy_directory
                "${dir}"
                "$<TARGET_FILE_DIR:${BB_TARGET}>/${BB_DESTINATION}/${name}"
        )

        install(
            DIRECTORY "${dir}"
            DESTINATION "${BB_DESTINATION}"
        )
    endforeach()

    foreach(file IN LISTS BB_FILES)
        add_custom_command(
            TARGET ${BB_TARGET}
            POST_BUILD
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                "${file}"
                "$<TARGET_FILE_DIR:${BB_TARGET}>/${BB_DESTINATION}"
        )

        install(
            FILES "${file}"
            DESTINATION "${BB_DESTINATION}"
        )
    endforeach()
endfunction()