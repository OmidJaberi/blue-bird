#include "repo/repo.h"

/* ---------------------------
 * Generic ops
 * --------------------------- */

static int repo_insert(BB_Repo *r, void *entity)
{
    return r->api->insert(r->handle, r->schema, entity);
}

static int repo_find_by_id(BB_Repo *r, void *out, int id)
{
    return r->api->find_by_id(r->handle, r->schema, out, id);
}

static int repo_update(BB_Repo *r, void *entity)
{
    return r->api->update(r->handle, r->schema, entity);
}

static int repo_remove(BB_Repo *r, int id)
{
    return r->api->remove(r->handle, r->schema, id);
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
    r->ops.find_by_id = repo_find_by_id;
    r->ops.update = repo_update;
    r->ops.remove = repo_remove;
}