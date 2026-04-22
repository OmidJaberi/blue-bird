#ifndef BB_REPO_H
#define BB_REPO_H

#include "persist/model.h"
#include "persist/schema.h"

typedef struct BB_Repo BB_Repo;

typedef struct {
    int (*insert)(BB_Repo *r, void *entity);
    int (*find_by_id)(BB_Repo *r, void *out, int id);
    int (*update)(BB_Repo *r, void *entity);
    int (*remove)(BB_Repo *r, int id);
} BB_RepoOps;

struct BB_Repo {
    const BB_ModelAPI *api;
    BB_ModelHandle *handle;
    BB_Schema *schema;

    BB_RepoOps ops;
};

/* constructor */
void bb_repo_init(BB_Repo *r,
                  const BB_ModelAPI *api,
                  BB_ModelHandle *handle,
                  BB_Schema *schema);

static inline int bb_repo_insert(BB_Repo *r, void *entity)
{
    return r->api->insert(r->handle, r->schema, entity);
}

static inline int bb_repo_find_by_id(BB_Repo *r, void *out, int id)
{
    return r->api->find_by_id(r->handle, r->schema, out, id);
}

static inline int bb_repo_update(BB_Repo *r, void *entity)
{
    return r->api->update(r->handle, r->schema, entity);
}

static inline int bb_repo_remove(BB_Repo *r, int id)
{
    return r->api->remove(r->handle, r->schema, id);
}

#define BB_DEFINE_REPO_TYPE(name, type) \
    typedef struct { BB_Repo base; } name; \
    static inline int name##_insert(name *r, type *e) { return bb_repo_insert(&r->base, e); } \
    static inline int name##_find(name *r, type *out, int id) { return bb_repo_find_by_id(&r->base, out, id); }

#endif