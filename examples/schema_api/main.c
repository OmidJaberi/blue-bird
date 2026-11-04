#include "api.h"

#include "Book_schema.generated.h"
#include "Note_schema.generated.h"

#include <blue-bird/persist/model/model_json.h>
#include <blue-bird/persist/model/model_sqlite.h>
#ifdef BB_WITH_POSTGRES
#include <blue-bird/persist/model/model_postgres.h>
#endif
#include <blue-bird/runtime/runtime.h>
#include <blue-bird/web/server.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Which backend stores the data is chosen at run time from BB_SCHEMA_API_DB:
 *
 *   (unset)                          SQLite, schema_api.db in the working directory
 *   sqlite:<path>                    SQLite
 *   json:<path>                      a JSON file
 *   postgresql://user:pass@host/db   PostgreSQL (build with -DBB_WITH_POSTGRES=ON)
 *
 * Only this function knows about the choice. */
static int open_backend(const char *spec, const bb_model_api_t **api, bb_model_handle_t **handle)
{
    const char *name = "sqlite";
    const char *location = "schema_api.db";

    if (spec && *spec)
    {
        location = NULL;

        if (strncmp(spec, "sqlite:", 7) == 0)
            location = spec + 7;
        else if (strncmp(spec, "json:", 5) == 0)
            name = "json", location = spec + 5;
        else if (strncmp(spec, "postgres://", 11) == 0 || strncmp(spec, "postgresql://", 13) == 0)
            name = "postgres", location = spec;
    }

    if (!location || !*location)
    {
        fprintf(stderr, "BB_SCHEMA_API_DB: expected sqlite:<path>, json:<path> or postgresql://...\n");
        return -1;
    }

    bb_model_register(bb_model_sqlite_api());
    bb_model_register(bb_model_json_api());
#ifdef BB_WITH_POSTGRES
    bb_model_register(bb_model_postgres_api());
#endif

    *api = bb_model_get(name);
    if (!*api)
    {
        fprintf(stderr, "This build has no '%s' backend (PostgreSQL needs -DBB_WITH_POSTGRES=ON).\n", name);
        return -1;
    }

    *handle = (*api)->open(location);
    if (!*handle)
    {
        fprintf(stderr, "Could not open the '%s' backend.\n", name);
        return -1;
    }

    return 0;
}

int main(void)
{
    const bb_model_api_t *model;
    bb_model_handle_t *handle;

    if (open_backend(getenv("BB_SCHEMA_API_DB"), &model, &handle) != 0)
        return 1;

    /* One line per entity. Everything else is generic. */
    if (api_register(&Book_schema, model, handle) != 0 ||
        api_register(&Note_schema, model, handle) != 0)
    {
        fprintf(stderr, "Could not register the schemas.\n");
        return 1;
    }

    bb_server_t *server = bb_server_create(8080);

    /* Routes are matched in order, so the fixed one comes first. */
    bb_server_add_route(server, "GET",    "/",              api_describe);
    bb_server_add_route(server, "GET",    "/:resource",     api_list);
    bb_server_add_route(server, "POST",   "/:resource",     api_create);
    bb_server_add_route(server, "GET",    "/:resource/:id", api_get);
    bb_server_add_route(server, "PUT",    "/:resource/:id", api_replace);
    bb_server_add_route(server, "DELETE", "/:resource/:id", api_delete);

    bb_server_start(server);
    bb_runtime_run_default();
    return 0;
}
