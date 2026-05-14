#include "blue-bird/persist/repo.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------
 * Init
 * --------------------------- */

void bb_repo_init(bb_repo_t *r,
                  const bb_model_api_t *api,
                  bb_model_handle_t *handle,
                  bb_schema_t *schema)
{
    r->api = api;
    r->handle = handle;
    r->schema = schema;
}

int bb_repo_filter(bb_repo_t *repo, void **out_array, size_t *out_count, bb_filter_cb fn, void *ctx)
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
