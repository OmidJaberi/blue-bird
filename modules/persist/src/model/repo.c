#include "blue-bird/persist/repo.h"

#include <stdlib.h>
#include <string.h>

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
}

int bb_repo_filter(BB_Repo *repo, void **out_array, size_t *out_count, BB_FilterFn fn, void *ctx)
{
    if (!repo || !out_array || !out_count || !fn)
    {
        return -1;
    }

    void *all = NULL;
    size_t total = 0;

    if (bb_repo_find_all(repo, &all, &total) != 0)
    {
        return -1;
    }

    if (total == 0)
    {
        *out_array = NULL;
        *out_count = 0;

        return 0;
    }

    void *filtered = calloc(total, repo->schema->struct_size);

    if (!filtered)
    {
        free(all);
        return -1;
    }

    size_t count = 0;

    for (size_t i = 0; i < total; i++)
    {
        void *entity = (char *)all + (i * repo->schema->struct_size);

        if (fn(entity, ctx))
        {
            memcpy(
                (char *)filtered +
                (count * repo->schema->struct_size),

                entity,

                repo->schema->struct_size
            );

            count++;
        }
    }

    free(all);

    if (count == 0)
    {
        free(filtered);

        *out_array = NULL;
        *out_count = 0;

        return 0;
    }

    *out_array = filtered;
    *out_count = count;

    return 0;
}
