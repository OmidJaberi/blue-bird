add_executable(example_todo
    main.c

    src/app_handlers.c
    src/app_middleware.c
    src/app_repo.c

    schemas/generated/Task_schema.generated.c
)

target_include_directories(example_todo
    PRIVATE
        include
        schemas/generated
)

target_link_libraries(example_todo
    PRIVATE
        bluebird
)

bb_add_assets(
    TARGET example_todo
    DIRECTORIES
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
)

# ---------------------------
# Schema codegen
#
# schemas/generated/*.generated.{h,c} are committed (see
# docs/persist/... codegen notes) -- they're not rebuilt automatically.
# `make regenerate_todo_schemas` regenerates them from
# schemas/*.schema.json after you edit a manifest; `ctest` verifies via
# `bb-codegen check` that nobody forgot to run it and commit the result.
# ---------------------------

if(BUILD_TOOLS)
    add_custom_target(regenerate_todo_schemas
        COMMAND
            $<TARGET_FILE:bb-codegen> generate
                --out ${CMAKE_CURRENT_SOURCE_DIR}/schemas/generated
                ${CMAKE_CURRENT_SOURCE_DIR}/schemas/task.schema.json
        DEPENDS
            bb-codegen
        WORKING_DIRECTORY
            ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT
            "Regenerating examples/todo/schemas/generated/ from schemas/*.schema.json"
    )

    if(BUILD_TESTS)
        add_test(
            NAME "examples.todo_schema_check"
            COMMAND
                $<TARGET_FILE:bb-codegen> check
                    --out ${CMAKE_CURRENT_SOURCE_DIR}/schemas/generated
                    ${CMAKE_CURRENT_SOURCE_DIR}/schemas/task.schema.json
        )
    endif()
endif()