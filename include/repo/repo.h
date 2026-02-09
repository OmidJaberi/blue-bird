#ifndef BB_REPO_H
#define BB_REPO_H

#include <stdint.h>
#include <stddef.h>
#include "persist/persist.h"

/*
 * Repo = record-oriented adapter over persist (KV store)
 *
 * It maps:
 *   (nspace, id) -> key string -> opaque bytes
 *
 * Repo knows NOTHING about:
 *   - HTTP
 *   - JSON
 *   - schemas
 *   - queries
 */

typedef struct bb_repo bb_repo_t;

typedef enum {
    BB_REPO_OK = 0,
    BB_REPO_NOT_FOUND,
    BB_REPO_CONFLICT,
    BB_REPO_ERROR
} bb_repo_status_t;

/* serialization hooks (provided by application) */
typedef int (*bb_repo_encode_fn)(
    const void *record,
    void       **out_buf,
    size_t     *out_size
);

typedef int (*bb_repo_decode_fn)(
    const void *buf,
    size_t      size,
    void       *out_record
);

/* lifecycle */
bb_repo_t *bb_repo_create(
    const char *nspace,
    bb_repo_encode_fn encode,
    bb_repo_decode_fn decode
);

void bb_repo_destroy(bb_repo_t *repo);

/* operations */
bb_repo_status_t bb_repo_put(
    bb_repo_t *repo,
    uint64_t   id,
    const void *record
);

bb_repo_status_t bb_repo_get(
    bb_repo_t *repo,
    uint64_t   id,
    void       *out_record
);

bb_repo_status_t bb_repo_delete(
    bb_repo_t *repo,
    uint64_t   id
);

#endif // BB_REPO_H
