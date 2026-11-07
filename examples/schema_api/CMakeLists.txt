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
# Tests
#
# The same end-to-end test runs on each persistence backend (HTTP requests
# against the real handlers), so passing on all of them is the check that
# the backends agree. The PostgreSQL run skips itself unless the build has
# -DBB_WITH_POSTGRES=ON and BB_TEST_POSTGRES_URI points at a database.
# Each run has its own port, none shared with another test, so ctest can run
# them in parallel.
# ---------------------------

if(BUILD_TESTS)
    add_executable(schema_api_test
        tests/test_api.c

        src/api.c

        ${SCHEMA_API_GENERATED}
    )

    target_include_directories(schema_api_test
        PRIVATE
            include
            schemas/generated
    )

    target_link_libraries(schema_api_test
        PRIVATE
            bluebird
            ${BLUEBIRD_THREADS_TARGET}
    )

    foreach(backend_port json:19181 sqlite:19182 postgres:19183)
        string(REPLACE ":" ";" backend_port_list ${backend_port})
        list(GET backend_port_list 0 backend)
        list(GET backend_port_list 1 port)

        add_test(
            NAME "examples.schema_api_${backend}"
            COMMAND schema_api_test ${backend} ${port}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        )
    endforeach()
endif()

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
