#include "manifest.h"

#include <stdio.h>
#include <string.h>

#define BB_CODEGEN_MAX_GENERATORS 16

static struct {
    const char *kind;
    const bb_codegen_generator_t *generator;
} g_generators[BB_CODEGEN_MAX_GENERATORS];

static size_t g_generator_count = 0;

int bb_codegen_register_generator(const char *kind, const bb_codegen_generator_t *generator)
{
    if (!kind || !generator)
        return -1;

    if (bb_codegen_get_generator(kind))
        return -1;

    if (g_generator_count >= BB_CODEGEN_MAX_GENERATORS)
        return -1;

    g_generators[g_generator_count].kind = kind;
    g_generators[g_generator_count].generator = generator;
    g_generator_count++;

    return 0;
}

const bb_codegen_generator_t *bb_codegen_get_generator(const char *kind)
{
    for (size_t i = 0; i < g_generator_count; i++)
    {
        if (strcmp(g_generators[i].kind, kind) == 0)
            return g_generators[i].generator;
    }

    return NULL;
}

bb_json_t *bb_codegen_load_manifest(const char *path)
{
    bb_json_t *manifest = bb_json_load(path);

    if (!manifest)
    {
        fprintf(stderr, "bb-codegen: failed to read/parse manifest '%s'\n", path);
        return NULL;
    }

    if (bb_json_get_type(manifest) != BB_JSON_OBJECT)
    {
        fprintf(stderr, "bb-codegen: manifest '%s' is not a JSON object\n", path);
        bb_json_destroy(manifest);
        return NULL;
    }

    bb_json_t *version = bb_json_object_get_value(manifest, "version");
    bb_json_t *kind = bb_json_object_get_value(manifest, "kind");
    bb_json_t *name = bb_json_object_get_value(manifest, "name");

    if (!version || bb_json_get_type(version) != BB_JSON_INT)
    {
        fprintf(stderr, "bb-codegen: manifest '%s' is missing an integer \"version\"\n", path);
        bb_json_destroy(manifest);
        return NULL;
    }

    if (!kind || bb_json_get_type(kind) != BB_JSON_TEXT)
    {
        fprintf(stderr, "bb-codegen: manifest '%s' is missing a string \"kind\"\n", path);
        bb_json_destroy(manifest);
        return NULL;
    }

    if (!name || bb_json_get_type(name) != BB_JSON_TEXT)
    {
        fprintf(stderr, "bb-codegen: manifest '%s' is missing a string \"name\"\n", path);
        bb_json_destroy(manifest);
        return NULL;
    }

    return manifest;
}

int bb_codegen_run(const char *manifest_path, const char *out_dir, int check_only)
{
    bb_json_t *manifest = bb_codegen_load_manifest(manifest_path);
    if (!manifest)
        return 1;

    const char *kind = bb_json_get_value_text(bb_json_object_get_value(manifest, "kind"));
    const bb_codegen_generator_t *generator = bb_codegen_get_generator(kind);

    if (!generator)
    {
        fprintf(stderr, "bb-codegen: no generator registered for kind '%s' (from '%s')\n",
                kind, manifest_path);
        bb_json_destroy(manifest);
        return 1;
    }

    int rc = check_only
        ? generator->check(manifest, manifest_path, out_dir)
        : generator->generate(manifest, manifest_path, out_dir);

    bb_json_destroy(manifest);
    return rc;
}
