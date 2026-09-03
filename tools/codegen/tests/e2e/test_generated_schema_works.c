/* This is the real proof for the codegen tool: not "does the text look
 * plausible" but "does the generated .h/.c actually compile, link, and
 * behave like a correct bb_schema_t when driven through a real backend".
 *
 * The Task_schema.generated.h/.c this file includes are NOT hand-written
 * -- they're produced by a CMake custom command (see tests/CMakeLists.txt)
 * that runs the built bb-codegen binary against fixtures/task.schema.json
 * at build time, before this file is compiled.
 */

#include "Task_schema.generated.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/persist/model/model_sqlite.h>
#include <blue-bird/persist/repo.h>

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void cleanup(const char *path)
{
    remove(path);
}

static void test_generated_struct_layout(void)
{
    printf("\tTesting generated struct/schema metadata is correct...\n");

    BB_ASSERT(Task_schema.field_count == 3);
    BB_ASSERT(Task_schema.primary_key_index == 0);
    BB_ASSERT(Task_schema.struct_size == sizeof(Task));
    BB_ASSERT(strcmp(Task_schema.name, "tasks") == 0);

    bb_field_t *id_field = bb_schema_find_field(&Task_schema, "id");
    BB_ASSERT(id_field != NULL);
    BB_ASSERT(id_field->type == BB_FIELD_UUID);
    BB_ASSERT(id_field->offset == offsetof(Task, id));
    BB_ASSERT(id_field->flags & BB_FIELD_PRIMARY_KEY);

    bb_field_t *name_field = bb_schema_find_field(&Task_schema, "name");
    BB_ASSERT(name_field != NULL);
    BB_ASSERT(name_field->type == BB_FIELD_STRING);
    BB_ASSERT(name_field->size == 64);
    BB_ASSERT(name_field->offset == offsetof(Task, name));
    BB_ASSERT(name_field->flags & BB_FIELD_REQUIRED);

    bb_field_t *status_field = bb_schema_find_field(&Task_schema, "status");
    BB_ASSERT(status_field != NULL);
    BB_ASSERT(status_field->flags == BB_FIELD_NONE);

    BB_ASSERT(bb_schema_validate(&Task_schema) == 0);
}

static void test_generated_schema_works_end_to_end(void)
{
    printf("\tTesting generated schema through a real SQLite repo...\n");

    const char *db_path = "codegen_e2e_task.db";
    cleanup(db_path);

    const bb_model_api_t *api = bb_model_sqlite_api();
    bb_model_handle_t *h = api->open(db_path);
    BB_ASSERT(h != NULL);

    bb_repo_t repo;
    bb_repo_init(&repo, api, h, &Task_schema);

    Task t = {0};
    strncpy(t.name, "Write the codegen tool", sizeof(t.name) - 1);
    strncpy(t.status, "in_progress", sizeof(t.status) - 1);
    /* id left zeroed -- not exercising uuid generation here, just
     * proving the generated field layout round-trips through SQLite. */
    strncpy(t.id, "11111111-1111-1111-1111-111111111111", sizeof(t.id));

    BB_ASSERT(bb_repo_insert(&repo, &t) == 0);

    Task out = {0};
    BB_ASSERT(bb_repo_find_by_pk(&repo, &out, t.id) == 0);
    BB_ASSERT(strcmp(out.name, "Write the codegen tool") == 0);
    BB_ASSERT(strcmp(out.status, "in_progress") == 0);

    strncpy(t.status, "done", sizeof(t.status) - 1);
    BB_ASSERT(bb_repo_update(&repo, &t) == 0);

    memset(&out, 0, sizeof(out));
    BB_ASSERT(bb_repo_find_by_pk(&repo, &out, t.id) == 0);
    BB_ASSERT(strcmp(out.status, "done") == 0);

    api->close(h);
    cleanup(db_path);
}

int main(void)
{
    printf("Running codegen end-to-end test (generated schema vs. real SQLite)...\n");

    test_generated_struct_layout();
    test_generated_schema_works_end_to_end();

    printf("All codegen end-to-end tests passed!\n");
    return 0;
}
