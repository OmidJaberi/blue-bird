set(BB_CODEGEN_TEMPLATES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/templates")

configure_file(
    src/build_config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/build_config.h
    @ONLY
)

add_library(bb_codegen_core
    src/manifest.c
    src/generators/persist_schema.c
)

target_include_directories(bb_codegen_core
    PUBLIC
        src
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)

target_link_libraries(bb_codegen_core
    PUBLIC
        bluebird_error
        bluebird_utils
        bluebird_template
)

add_executable(bb-codegen
    src/main.c
)

target_link_libraries(bb-codegen
    PRIVATE
        bb_codegen_core
)

if(BUILD_TESTS)
    add_subdirectory(tests)
endif()
