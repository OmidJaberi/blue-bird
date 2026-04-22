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

#endif