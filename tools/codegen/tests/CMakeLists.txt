bb_add_module_tests(
    codegen
    bb_codegen_core
)

# ---------------------------
# End-to-end proof
#
# Runs the built bb-codegen binary against a fixture manifest at build
# time, then compiles the *generated* .h/.c together with a test harness
# and links it against the real persist/sqlite backend -- this is what
# actually proves the generated code compiles, links, and behaves
# correctly, not just that its text looks plausible.
# ---------------------------

set(BB_CODEGEN_E2E_FIXTURE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/e2e/fixtures)
set(BB_CODEGEN_E2E_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/e2e_generated)

add_custom_command(
    OUTPUT
        ${BB_CODEGEN_E2E_OUT_DIR}/Task_schema.generated.h
        ${BB_CODEGEN_E2E_OUT_DIR}/Task_schema.generated.c
    COMMAND
        $<TARGET_FILE:bb-codegen> generate
            --out ${BB_CODEGEN_E2E_OUT_DIR}
            ${BB_CODEGEN_E2E_FIXTURE_DIR}/task.schema.json
    DEPENDS
        bb-codegen
        ${BB_CODEGEN_E2E_FIXTURE_DIR}/task.schema.json
    COMMENT
        "bb-codegen: regenerating fixture schema for codegen e2e test"
)

add_executable(codegen_test_generated_schema_works
    e2e/test_generated_schema_works.c
    ${BB_CODEGEN_E2E_OUT_DIR}/Task_schema.generated.c
)

target_include_directories(codegen_test_generated_schema_works
    PRIVATE
        ${BB_CODEGEN_E2E_OUT_DIR}
)

target_link_libraries(codegen_test_generated_schema_works
    PRIVATE
        bluebird_persist
        bluebird_error
)

set_target_properties(codegen_test_generated_schema_works PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/e2e"
)

add_test(
    NAME "codegen.test_generated_schema_works"
    COMMAND $<TARGET_FILE:codegen_test_generated_schema_works>
)
