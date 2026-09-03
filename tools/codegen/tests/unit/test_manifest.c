#include "manifest.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/json.h>

#include <stdio.h>
#include <string.h>

static void write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

/* ---------------------------
 * Generator registry
 * --------------------------- */

static int fake_generate(bb_json_t *manifest, const char *manifest_path, const char *out_dir)
{
    (void)manifest; (void)manifest_path; (void)out_dir;
    return 0;
}

static int fake_check(bb_json_t *manifest, const char *manifest_path, const char *out_dir)
{
    (void)manifest; (void)manifest_path; (void)out_dir;
    return 0;
}

static void test_register_and_get_generator(void)
{
    printf("\tTesting generator registration and lookup...\n");

    static const bb_codegen_generator_t g = { .generate = fake_generate, .check = fake_check };

    BB_ASSERT(bb_codegen_get_generator("test.unregistered") == NULL);
    BB_ASSERT(bb_codegen_register_generator("test.kind_a", &g) == 0);
    BB_ASSERT(bb_codegen_get_generator("test.kind_a") == &g);

    /* Registering the same kind twice is rejected. */
    BB_ASSERT(bb_codegen_register_generator("test.kind_a", &g) != 0);
}

/* ---------------------------
 * Manifest loading/validation
 * --------------------------- */

static void test_load_valid_manifest(void)
{
    printf("\tTesting loading a valid manifest...\n");

    const char *path = "test_manifest_valid.json";
    write_text_file(path,
        "{ \"version\": 1, \"kind\": \"test.kind_a\", \"name\": \"Thing\" }");

    bb_json_t *m = bb_codegen_load_manifest(path);
    BB_ASSERT(m != NULL);

    bb_json_t *kind = bb_json_object_get_value(m, "kind");
    BB_ASSERT(kind != NULL);
    BB_ASSERT(strcmp(bb_json_get_value_text(kind), "test.kind_a") == 0);

    bb_json_destroy(m);
}

static void test_load_missing_file(void)
{
    printf("\tTesting loading a missing manifest file...\n");

    bb_json_t *m = bb_codegen_load_manifest("does_not_exist_at_all.json");
    BB_ASSERT(m == NULL);
}

static void test_load_rejects_missing_version(void)
{
    printf("\tTesting manifest without \"version\" is rejected...\n");

    const char *path = "test_manifest_no_version.json";
    write_text_file(path, "{ \"kind\": \"test.kind_a\", \"name\": \"Thing\" }");

    bb_json_t *m = bb_codegen_load_manifest(path);
    BB_ASSERT(m == NULL);
}

static void test_load_rejects_missing_kind(void)
{
    printf("\tTesting manifest without \"kind\" is rejected...\n");

    const char *path = "test_manifest_no_kind.json";
    write_text_file(path, "{ \"version\": 1, \"name\": \"Thing\" }");

    bb_json_t *m = bb_codegen_load_manifest(path);
    BB_ASSERT(m == NULL);
}

static void test_load_rejects_missing_name(void)
{
    printf("\tTesting manifest without \"name\" is rejected...\n");

    const char *path = "test_manifest_no_name.json";
    write_text_file(path, "{ \"version\": 1, \"kind\": \"test.kind_a\" }");

    bb_json_t *m = bb_codegen_load_manifest(path);
    BB_ASSERT(m == NULL);
}

static void test_load_rejects_non_object(void)
{
    printf("\tTesting manifest that isn't a JSON object is rejected...\n");

    const char *path = "test_manifest_not_object.json";
    write_text_file(path, "[ 1, 2, 3 ]");

    bb_json_t *m = bb_codegen_load_manifest(path);
    BB_ASSERT(m == NULL);
}

/* ---------------------------
 * End-to-end dispatch (bb_codegen_run)
 * --------------------------- */

static int seen_generate = 0;
static int seen_check = 0;

static int spy_generate(bb_json_t *manifest, const char *manifest_path, const char *out_dir)
{
    (void)manifest; (void)manifest_path; (void)out_dir;
    seen_generate = 1;
    return 0;
}

static int spy_check(bb_json_t *manifest, const char *manifest_path, const char *out_dir)
{
    (void)manifest; (void)manifest_path; (void)out_dir;
    seen_check = 1;
    return 0;
}

static void test_run_dispatches_to_correct_generator(void)
{
    printf("\tTesting bb_codegen_run dispatches by \"kind\"...\n");

    static const bb_codegen_generator_t g = { .generate = spy_generate, .check = spy_check };
    BB_ASSERT(bb_codegen_register_generator("test.kind_b", &g) == 0);

    const char *path = "test_manifest_dispatch.json";
    write_text_file(path, "{ \"version\": 1, \"kind\": \"test.kind_b\", \"name\": \"Thing\" }");

    seen_generate = 0;
    seen_check = 0;

    BB_ASSERT(bb_codegen_run(path, "out", 0) == 0);
    BB_ASSERT(seen_generate == 1);
    BB_ASSERT(seen_check == 0);

    BB_ASSERT(bb_codegen_run(path, "out", 1) == 0);
    BB_ASSERT(seen_check == 1);
}

static void test_run_unknown_kind_fails(void)
{
    printf("\tTesting bb_codegen_run fails for an unregistered kind...\n");

    const char *path = "test_manifest_unknown_kind.json";
    write_text_file(path, "{ \"version\": 1, \"kind\": \"test.does_not_exist\", \"name\": \"Thing\" }");

    BB_ASSERT(bb_codegen_run(path, "out", 0) != 0);
}

int main(void)
{
    printf("Running manifest unit tests...\n");

    test_register_and_get_generator();

    test_load_valid_manifest();
    test_load_missing_file();
    test_load_rejects_missing_version();
    test_load_rejects_missing_kind();
    test_load_rejects_missing_name();
    test_load_rejects_non_object();

    test_run_dispatches_to_correct_generator();
    test_run_unknown_kind_fails();

    printf("All manifest tests passed!\n");
    return 0;
}
