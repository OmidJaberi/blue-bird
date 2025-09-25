file(GLOB EXAMPLE_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*")

foreach(dir ${EXAMPLE_DIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${dir}")
        add_subdirectory(${dir})
    endif()
endforeach()