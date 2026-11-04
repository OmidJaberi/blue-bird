# Every generated schema in schemas/generated/ is compiled in, so adding an
# entity needs no change here.
file(GLOB SCHEMA_API_GENERATED CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/schemas/generated/*.generated.c")

add_executable(example_schema_api
    main.c

    src/api.c

    ${SCHEMA_API_GENERATED}
)

target_include_directories(example_schema_api
    PRIVATE
        include
        schemas/generated
)

target_link_libraries(example_schema_api
    PRIVATE
        bluebird
)

# ---------------------------
# Schema codegen
#
# schemas/generated/*.generated.{h,c} are committed. After adding or editing
# a schemas/*.schema.json manifest, run `make regenerate_schema_api_schemas`
# and commit the result; `ctest` runs `bb-codegen check` to catch a forgotten
# regeneration.
# ---------------------------

if(BUILD_TOOLS)
    file(GLOB SCHEMA_API_MANIFESTS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/schemas/*.schema.json")

    add_custom_target(regenerate_schema_api_schemas
        COMMAND
            $<TARGET_FILE:bb-codegen> generate
                --out ${CMAKE_CURRENT_SOURCE_DIR}/schemas/generated
                ${SCHEMA_API_MANIFESTS}
        DEPENDS
            bb-codegen
        WORKING_DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT
            "Regenerating examples/schema_api/schemas/generated/ from schemas/*.schema.json"
    )

    if(BUILD_TESTS)
        add_test(
            NAME "examples.schema_api_schema_check"
            COMMAND
                $<TARGET_FILE:bb-codegen> check
                    --out ${CMAKE_CURRENT_SOURCE_DIR}/schemas/generated
                    ${SCHEMA_API_MANIFESTS}
        )
    endif()
endif()
