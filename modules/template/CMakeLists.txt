add_library(bluebird_template
    src/ast.c
    src/parser.c
    src/renderer.c
    src/string_builder.c
    src/template.c
)

target_include_directories(bluebird_template
    PUBLIC
        include
    PRIVATE
        internal
)

target_link_libraries(bluebird_template
    PUBLIC
        bluebird_error
    PRIVATE
        bluebird_utils
)

add_library(BlueBird::template ALIAS bluebird_template)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
