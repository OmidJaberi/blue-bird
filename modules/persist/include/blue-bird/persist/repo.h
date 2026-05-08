#ifndef BB_REPO_H
#define BB_REPO_H

#include "blue-bird/persist/model.h"
#include "blue-bird/persist/schema.h"

typedef struct BB_Repo BB_Repo;

typedef struct {
    int (*insert)(BB_Repo *r, void *entity);
    int (*find_by_pk)(BB_Repo *r, void *out, const void *key);
    int (*update)(BB_Repo *r, void *entity);
    int (*remove)(BB_Repo *r, const void *key);
} BB_RepoOps;

struct BB_Repo {
    const BB_ModelAPI *api;
    BB_ModelHandle *handle;
    BB_Schema *schema;

    BB_RepoOps ops;
};

typedef int (*BB_FilterFn)(
    const void *entity,
    void *ctx
);

int bb_repo_filter(
    BB_Repo *repo,
    void **out_array,
    size_t *out_count,
    BB_FilterFn fn,
    void *ctx
);

/* constructor */
void bb_repo_init(BB_Repo *r,
                  const BB_ModelAPI *api,
                  BB_ModelHandle *handle,
                  BB_Schema *schema);

static inline int bb_repo_insert(BB_Repo *r, void *entity)
{
    return r->api->insert(r->handle, r->schema, entity);
}

static inline int bb_repo_find_by_pk(BB_Repo *r, void *out, const void *key)
{
    return r->api->find_by_pk(r->handle, r->schema, out, key);
}

static inline int bb_repo_update(BB_Repo *r, void *entity)
{
    return r->api->update(r->handle, r->schema, entity);
}

static inline int bb_repo_remove(BB_Repo *r, const void *key)
{
    return r->api->remove(r->handle, r->schema, key);
}

static inline int bb_repo_find_all(BB_Repo *r, void **out_array, size_t *out_count)
{
    return r->api->find_all(r->handle, r->schema, out_array, out_count);
}

static inline int bb_repo_find_first_by_field(BB_Repo *r, void *out, const char *field, const void *value)
{
    return r->api->find_first_by_field(r->handle, r->schema, out, field, value);
}

#define BB_DEFINE_REPO_TYPE(name, type) \
    typedef struct { BB_Repo base; } name; \
    static inline int name##_insert(name *r, type *e) { return bb_repo_insert(&r->base, e); } \
    static inline int name##_find(name *r, type *out, const void *key) { return bb_repo_find_by_pk(&r->base, out, key); }

#endif