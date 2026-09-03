#ifndef BB_CODEGEN_MANIFEST_H
#define BB_CODEGEN_MANIFEST_H

#include <blue-bird/utils/json.h>

/* ---------------------------
 * bb-codegen manifest layer
 *
 * Every manifest is a JSON object carrying a "kind" discriminator, the
 * ARXML-style bit that keeps this tool generic:
 *
 *   { "version": 1, "kind": "persist.schema", "name": "...", ... }
 *
 * This layer only knows how to load the file, check version/kind/name
 * are present, and hand the raw manifest off to whichever generator is
 * registered for that "kind". It has zero knowledge of what a
 * "persist.schema" manifest actually contains -- that's the generator's
 * job. New manifest kinds (web.route, security.policy, ...) plug in by
 * registering a generator, never by touching this file.
 * --------------------------- */

typedef struct {
    /* Runs a validated manifest, writing generated output into out_dir.
     * Returns 0 on success, non-zero on failure (with a message already
     * printed to stderr). */
    int (*generate)(bb_json_t *manifest, const char *manifest_path, const char *out_dir);

    /* Regenerates in memory and compares against whatever already
     * exists on disk in out_dir, without writing anything. Returns 0 if
     * everything is up to date, non-zero (with a description of what's
     * stale/missing printed to stderr) otherwise. Used by `bb-codegen
     * check` in CI to catch a manifest that was edited without
     * regenerating. */
    int (*check)(bb_json_t *manifest, const char *manifest_path, const char *out_dir);
} bb_codegen_generator_t;

/* Registers a generator for a manifest "kind". Returns 0 on success, -1
 * if the kind is already registered or the registry is full. */
int bb_codegen_register_generator(const char *kind, const bb_codegen_generator_t *generator);

/* Looks up a previously registered generator by kind. Returns NULL if
 * none is registered. */
const bb_codegen_generator_t *bb_codegen_get_generator(const char *kind);

/* Loads and minimally validates a manifest file (valid JSON, has
 * "version"/"kind"/"name" fields). Returns NULL on failure (with a
 * message printed to stderr). Caller owns the returned bb_json_t and
 * must bb_json_destroy() it. */
bb_json_t *bb_codegen_load_manifest(const char *path);

/* Convenience: load a manifest and run generate()/check() for whichever
 * generator matches its "kind". Returns 0 on success, non-zero
 * otherwise (unknown kind, load failure, or the generator itself
 * failing). */
int bb_codegen_run(const char *manifest_path, const char *out_dir, int check_only);

#endif //BB_CODEGEN_MANIFEST_H
