#include "generators/persist_schema.h"
#include "manifest.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/json.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_json_manifest(const char *path, bb_json_t *manifest)
{
    BB_ASSERT(!BB_FAILED(bb_json_dump(manifest, path)));
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    BB_ASSERT(f != NULL);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)size + 1);
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static bb_json_t *make_task_manifest(void)
{
    /* Mirrors examples/todo's hand-written Task schema: a uuid PK plus
     * two required strings. */
    return BB_JSON(
        OBJ(
            KEY("version", INTV(1)),
            KEY("kind", TEXTV("persist.schema")),
            KEY("name", TEXTV("Task")),
            KEY("table", TEXTV("tasks")),
            KEY("fields", ARR(
                OBJ(
                    KEY("name", TEXTV("id")),
                    KEY("type", TEXTV("uuid")),
                    KEY("primary_key", BOOLV(true))
                ),
                OBJ(
                    KEY("name", TEXTV("name")),
                    KEY("type", TEXTV("string")),
                    KEY("size", INTV(64)),
                    KEY("required", BOOLV(true))
                ),
                OBJ(
                    KEY("name", TEXTV("status")),
                    KEY("type", TEXTV("string")),
                    KEY("size", INTV(64))
                )
            ))
        )
    );
}

static void test_generate_produces_expected_struct_and_schema(void)
{
    printf("\tTesting generate() produces the expected struct/schema...\n");

    bb_json_t *manifest = make_task_manifest();
    write_json_manifest("test_task.schema.json", manifest);

    int rc = bb_codegen_persist_schema_generator.generate(manifest, "test_task.schema.json", "test_out");
    BB_ASSERT(rc == 0);

    char *header = read_file("test_out/Task_schema.generated.h");
    char *source = read_file("test_out/Task_schema.generated.c");

    /* Struct fields, in declaration form (offsets computed by the C
     * compiler, not by us -- that's the whole point). */
    BB_ASSERT(strstr(header, "bb_uuid_t id;") != NULL);
    BB_ASSERT(strstr(header, "char name[64];") != NULL);
    BB_ASSERT(strstr(header, "char status[64];") != NULL);
    BB_ASSERT(strstr(header, "#include <blue-bird/utils/uuid.h>") != NULL);
    BB_ASSERT(strstr(header, "extern bb_field_t Task_fields[];") != NULL);
    BB_ASSERT(strstr(header, "extern bb_schema_t Task_schema;") != NULL);

    BB_ASSERT(strstr(source, "offsetof(Task, id)") != NULL);
    BB_ASSERT(strstr(source, "offsetof(Task, name)") != NULL);
    BB_ASSERT(strstr(source, "offsetof(Task, status)") != NULL);
    BB_ASSERT(strstr(source, ".type = BB_FIELD_UUID") != NULL);
    BB_ASSERT(strstr(source, ".flags = BB_FIELD_PRIMARY_KEY") != NULL);
    BB_ASSERT(strstr(source, ".flags = BB_FIELD_REQUIRED") != NULL);
    BB_ASSERT(strstr(source, ".name = \"tasks\"") != NULL);
    BB_ASSERT(strstr(source, ".struct_size = sizeof(Task)") != NULL);
    BB_ASSERT(strstr(source, ".primary_key_index = 0") != NULL);

    free(header);
    free(source);
    bb_json_destroy(manifest);
}

static void test_check_reports_up_to_date_after_generate(void)
{
    printf("\tTesting check() passes right after generate()...\n");

    bb_json_t *manifest = make_task_manifest();

    BB_ASSERT(bb_codegen_persist_schema_generator.generate(manifest, "test_task.schema.json", "test_out_check") == 0);
    BB_ASSERT(bb_codegen_persist_schema_generator.check(manifest, "test_task.schema.json", "test_out_check") == 0);

    bb_json_destroy(manifest);
}

static void test_check_detects_drift(void)
{
    printf("\tTesting check() detects a manifest edited without regenerating...\n");

    bb_json_t *manifest = make_task_manifest();
    BB_ASSERT(bb_codegen_persist_schema_generator.generate(manifest, "test_task.schema.json", "test_out_drift") == 0);
    BB_ASSERT(bb_codegen_persist_schema_generator.check(manifest, "test_task.schema.json", "test_out_drift") == 0);

    /* Simulate someone adding a field to the manifest and forgetting to
     * regenerate: check() against the *new* manifest but the *old*
     * output on disk must fail. */
    bb_json_t *changed = BB_JSON(
        OBJ(
            KEY("version", INTV(1)),
            KEY("kind", TEXTV("persist.schema")),
            KEY("name", TEXTV("Task")),
            KEY("table", TEXTV("tasks")),
            KEY("fields", ARR(
                OBJ(KEY("name", TEXTV("id")), KEY("type", TEXTV("uuid")), KEY("primary_key", BOOLV(true))),
                OBJ(KEY("name", TEXTV("name")), KEY("type", TEXTV("string")), KEY("size", INTV(64))),
                OBJ(KEY("name", TEXTV("status")), KEY("type", TEXTV("string")), KEY("size", INTV(64))),
                OBJ(KEY("name", TEXTV("priority")), KEY("type", TEXTV("int")))
            ))
        )
    );

    BB_ASSERT(bb_codegen_persist_schema_generator.check(changed, "test_task.schema.json", "test_out_drift") != 0);

    bb_json_destroy(manifest);
    bb_json_destroy(changed);
}

static void test_check_detects_missing_output(void)
{
    printf("\tTesting check() fails when output was never generated...\n");

    bb_json_t *manifest = make_task_manifest();
    BB_ASSERT(bb_codegen_persist_schema_generator.check(manifest, "test_task.schema.json", "test_out_never_generated") != 0);
    bb_json_destroy(manifest);
}

/* ---------------------------
 * Validation failures
 * --------------------------- */

static void test_rejects_no_primary_key(void)
{
    printf("\tTesting a schema with no primary_key field is rejected...\n");

    bb_json_t *manifest = BB_JSON(
        OBJ(
            KEY("version", INTV(1)),
            KEY("kind", TEXTV("persist.schema")),
            KEY("name", TEXTV("Widget")),
            KEY("fields", ARR(
                OBJ(KEY("name", TEXTV("id")), KEY("type", TEXTV("int")))
            ))
        )
    );

    BB_ASSERT(bb_codegen_persist_schema_generator.generate(manifest, "widget.schema.json", "test_out_bad") != 0);
    bb_json_destroy(manifest);
}

static void test_rejects_two_primary_keys(void)
{
    printf("\tTesting a schema with two primary_key fields is rejected...\n");

    bb_json_t *manifest = BB_JSON(
        OBJ(
            KEY("version", INTV(1)),
            KEY("kind", TEXTV("persist.schema")),
            KEY("name", TEXTV("Widget")),
            KEY("fields", ARR(
                OBJ(KEY("name", TEXTV("id")), KEY("type", TEXTV("int")), KEY("primary_key", BOOLV(true))),
                OBJ(KEY("name", TEXTV("other_id")), KEY("type", TEXTV("int")), KEY("primary_key", BOOLV(true)))
            ))
        )
    );

    BB_ASSERT(bb_codegen_persist_schema_generator.generate(manifest, "widget.schema.json", "test_out_bad") != 0);
    bb_json_destroy(manifest);
}

static void test_rejects_duplicate_field_names(void)
{
    printf("\tTesting a schema with duplicate field names is rejected...\n");

    bb_json_t *manifest = BB_JSON(
        OBJ(
            KEY("version", INTV(1)),
            KEY("kind", TEXTV("persist.schema")),
            KEY("name", TEXTV("Widget")),
            KEY("fields", ARR(
                OBJ(KEY("name", TEXTV("id")), KEY("type", TEXTV("int")), KEY("primary_key", BOOLV(true))),
                OBJ(KEY("name", TEXTV("id")), KEY("type", TEXTV("int")))
            ))
        )
    );

    BB_ASSERT(bb_codegen_persist_schema_generator.generate(manifest, "widget.schema.json", "test_out_bad") != 0);
    bb_json_destroy(manifest);
}

static void test_rejects_string_without_size(void)
{
    printf("\tTesting a string field without \"size\" is rejected...\n");

    bb_json_t *manifest = BB_JSON(
        OBJ(
            KEY("version", INTV(1)),
            KEY("kind", TEXTV("persist.schema")),
            KEY("name", TEXTV("Widget")),
            KEY("fields", ARR(
                OBJ(KEY("name", TEXTV("id")), KEY("type", TEXTV("int")), KEY("primary_key", BOOLV(true))),
                OBJ(KEY("name", TEXTV("label")), KEY("type", TEXTV("string")))
            ))
        )
    );

    BB_ASSERT(bb_codegen_persist_schema_generator.generate(manifest, "widget.schema.json", "test_out_bad") != 0);
    bb_json_destroy(manifest);
}

static void test_rejects_unknown_type(void)
{
    printf("\tTesting an unknown field type is rejected...\n");

    bb_json_t *manifest = BB_JSON(
        OBJ(
            KEY("version", INTV(1)),
            KEY("kind", TEXTV("persist.schema")),
            KEY("name", TEXTV("Widget")),
            KEY("fields", ARR(
                OBJ(KEY("name", TEXTV("id")), KEY("type", TEXTV("float")), KEY("primary_key", BOOLV(true)))
            ))
        )
    );

    BB_ASSERT(bb_codegen_persist_schema_generator.generate(manifest, "widget.schema.json", "test_out_bad") != 0);
    bb_json_destroy(manifest);
}

static void test_rejects_invalid_identifier_name(void)
{
    printf("\tTesting a schema \"name\" that isn't a valid C identifier is rejected...\n");

    bb_json_t *manifest = BB_JSON(
        OBJ(
            KEY("version", INTV(1)),
            KEY("kind", TEXTV("persist.schema")),
            KEY("name", TEXTV("not a valid identifier")),
            KEY("fields", ARR(
                OBJ(KEY("name", TEXTV("id")), KEY("type", TEXTV("int")), KEY("primary_key", BOOLV(true)))
            ))
        )
    );

    BB_ASSERT(bb_codegen_persist_schema_generator.generate(manifest, "widget.schema.json", "test_out_bad") != 0);
    bb_json_destroy(manifest);
}

static void test_check_agrees_with_generate_across_different_manifest_paths(void)
{
    printf("\tTesting generate()/check() agree regardless of how the manifest path is spelled...\n");

    /* Regression test: the generated comment used to embed the literal
     * manifest_path argument, so generating with a relative path and
     * then checking with an absolute (or differently-relative) path to
     * the *same* manifest would report false staleness. Only the
     * basename may appear in generated output now. */
    bb_json_t *manifest = make_task_manifest();

    BB_ASSERT(bb_codegen_persist_schema_generator.generate(
        manifest, "schemas/test_task.schema.json", "test_out_path_invariance") == 0);

    BB_ASSERT(bb_codegen_persist_schema_generator.check(
        manifest, "/some/other/absolute/path/test_task.schema.json", "test_out_path_invariance") == 0);

    BB_ASSERT(bb_codegen_persist_schema_generator.check(
        manifest, "./test_task.schema.json", "test_out_path_invariance") == 0);

    bb_json_destroy(manifest);
}

int main(void)
{
    printf("Running persist.schema generator unit tests...\n");

    test_generate_produces_expected_struct_and_schema();
    test_check_reports_up_to_date_after_generate();
    test_check_detects_drift();
    test_check_detects_missing_output();
    test_check_agrees_with_generate_across_different_manifest_paths();

    test_rejects_no_primary_key();
    test_rejects_two_primary_keys();
    test_rejects_duplicate_field_names();
    test_rejects_string_without_size();
    test_rejects_unknown_type();
    test_rejects_invalid_identifier_name();

    printf("All persist.schema generator tests passed!\n");
    return 0;
}
