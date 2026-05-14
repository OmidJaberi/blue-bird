#ifndef BB_REPO_H
#define BB_REPO_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/persist/model.h"
#include "blue-bird/persist/schema.h"

typedef struct bb_repo_t bb_repo_t;

struct bb_repo_t {
    const bb_model_api_t *api;
    bb_model_handle_t *handle;
    bb_schema_t *schema;
};

void bb_repo_init(
    bb_repo_t *r,
    const bb_model_api_t *api,
    bb_model_handle_t *handle,
    bb_schema_t *schema
);

typedef int (*bb_filter_cb)(
    const void *entity,
    void *ctx
);

int bb_repo_filter(
    bb_repo_t *repo,
    void **out_array,
    size_t *out_count,
    bb_filter_cb fn,
    void *ctx
);

// Model Ops

static inline int bb_repo_insert(bb_repo_t *r, void *entity)
{
    return r->api->insert(r->handle, r->schema, entity);
}

static inline int bb_repo_find_by_pk(bb_repo_t *r, void *out, const void *key)
{
    return r->api->find_by_pk(r->handle, r->schema, out, key);
}

static inline int bb_repo_update(bb_repo_t *r, void *entity)
{
    return r->api->update(r->handle, r->schema, entity);
}

static inline int bb_repo_remove(bb_repo_t *r, const void *key)
{
    return r->api->remove(r->handle, r->schema, key);
}

static inline int bb_repo_find_all(bb_repo_t *r, void **out_array, size_t *out_count)
{
    return r->api->find_all(r->handle, r->schema, out_array, out_count);
}

static inline int bb_repo_find_first_by_field(bb_repo_t *r, void *out, const char *field, const void *value)
{
    return r->api->find_first_by_field(r->handle, r->schema, out, field, value);
}

#define BB_DEFINE_REPO_TYPE(name, type) \
    typedef struct { bb_repo_t base; } name; \
    static inline int name##_insert(name *r, type *e) { return bb_repo_insert(&r->base, e); } \
    static inline int name##_find(name *r, type *out, const void *key) { return bb_repo_find_by_pk(&r->base, out, key); }


#ifdef __cplusplus
}
#endif

#endif
