#include "repo/repo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/*
 * Internal representation
 *
 * Repo does NOT own the persist backend.
 * It uses the default persist configured by the app.
 */
struct bb_repo {
    char *nspace;
    bb_repo_encode_fn encode;
    bb_repo_decode_fn decode;
};

/* ------------------------- helpers ------------------------- */

static int make_key(
    const bb_repo_t *repo,
    uint64_t id,
    char *buf,
    size_t bufsize
) {
    int n = snprintf(buf, bufsize, "%s/%" PRIu64, repo->nspace, id);
    return (n < 0 || (size_t)n >= bufsize) ? -1 : 0;
}

/* ------------------------- lifecycle ------------------------- */

bb_repo_t *bb_repo_create(
    const char *nspace,
    bb_repo_encode_fn encode,
    bb_repo_decode_fn decode
) {
    if (!nspace || !encode || !decode)
        return NULL;

    bb_repo_t *repo = calloc(1, sizeof(*repo));
    if (!repo)
        return NULL;

    repo->nspace = strdup(nspace);
    if (!repo->nspace) {
        free(repo);
        return NULL;
    }

    repo->encode = encode;
    repo->decode = decode;

    return repo;
}

void bb_repo_destroy(bb_repo_t *repo) {
    if (!repo)
        return;

    free(repo->nspace);
    free(repo);
}

/* ------------------------- operations ------------------------- */

bb_repo_status_t bb_repo_put(
    bb_repo_t *repo,
    uint64_t id,
    const void *record
) {
    if (!repo || !record)
        return BB_REPO_ERROR;

    void *buf = NULL;
    size_t size = 0;

    if (repo->encode(record, &buf, &size) != 0 || !buf)
        return BB_REPO_ERROR;

    char key[256];
    if (make_key(repo, id, key, sizeof key) != 0) {
        free(buf);
        return BB_REPO_ERROR;
    }

    int rc = persist_save(key, buf, size);
    free(buf);

    return rc == 0 ? BB_REPO_OK : BB_REPO_ERROR;
}

bb_repo_status_t bb_repo_get(
    bb_repo_t *repo,
    uint64_t id,
    void *out_record
) {
    if (!repo || !out_record)
        return BB_REPO_ERROR;

    char key[256];
    if (make_key(repo, id, key, sizeof key) != 0)
        return BB_REPO_ERROR;

    /* v1: fixed buffer, explicit limitation */
    unsigned char buf[4096];

    int rc = persist_load(key, buf, sizeof buf);
    if (rc < 0)
        return BB_REPO_NOT_FOUND;

    if (repo->decode(buf, (size_t)rc, out_record) != 0)
        return BB_REPO_ERROR;

    return BB_REPO_OK;
}

bb_repo_status_t bb_repo_delete(
    bb_repo_t *repo,
    uint64_t id
) {
    if (!repo)
        return BB_REPO_ERROR;

    char key[256];
    if (make_key(repo, id, key, sizeof key) != 0)
        return BB_REPO_ERROR;

    int rc = persist_remove(key);
    return rc == 0 ? BB_REPO_OK : BB_REPO_NOT_FOUND;
}
