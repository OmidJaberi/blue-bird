#include "repo/repo.h"

/* ---------------------------
 * Generic ops
 * --------------------------- */

static int repo_insert(BB_Repo *r, void *entity)
{
    return r->api->insert(r->handle, r->schema, entity);
}

static int repo_find_by_pk(BB_Repo *r, void *out, const void *key)
{
    return r->api->find_by_pk(r->handle, r->schema, out, key);
}

static int repo_update(BB_Repo *r, void *entity)
{
    return r->api->update(r->handle, r->schema, entity);
}

static int repo_remove(BB_Repo *r, const void *key)
{
    return r->api->remove(r->handle, r->schema, key);
}

/* ---------------------------
 * Init
 * --------------------------- */

void bb_repo_init(BB_Repo *r,
                  const BB_ModelAPI *api,
                  BB_ModelHandle *handle,
                  BB_Schema *schema)
{
    r->api = api;
    r->handle = handle;
    r->schema = schema;

    r->ops.insert = repo_insert;
    r->ops.find_by_pk = repo_find_by_pk;
    r->ops.update = repo_update;
    r->ops.remove = repo_remove;
}