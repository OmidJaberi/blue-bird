#include "app_handlers.h"
#include "app_middleware.h"
#include "app_repo.h"

#include <blue-bird/runtime/runtime.h>
#include <blue-bird/web/server.h>
#include <blue-bird/log/console_logger.h>
#include <blue-bird/persist/model/model_json.h>
#include <blue-bird/persist/model/model_sqlite.h>
#ifdef BB_WITH_POSTGRES
#include <blue-bird/persist/model/model_postgres.h>
#endif
#include <blue-bird/utils/asset.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Which backend stores the tasks is chosen at run time from BB_TODO_DB:
 *
 *   (unset)                          SQLite, todo_sqlite.db next to the binary
 *   sqlite:<path>                    SQLite
 *   json:<path>                      a JSON file
 *   postgresql://user:pass@host/db   PostgreSQL (build with -DBB_WITH_POSTGRES=ON)
 *
 * Only this function knows about the choice; everything below it just uses
 * the repo. */
static int open_backend(const char *spec, const bb_model_api_t **api, bb_model_handle_t **handle)
{
    char default_path[1024];
    const char *name = "sqlite";
    const char *location = NULL;

    if (!spec || !*spec)
    {
        bb_asset_resolve_path("todo_sqlite.db", default_path, sizeof(default_path));
        location = default_path;
    }
    else if (strncmp(spec, "sqlite:", 7) == 0)
        location = spec + 7;
    else if (strncmp(spec, "json:", 5) == 0)
        name = "json", location = spec + 5;
    else if (strncmp(spec, "postgres://", 11) == 0 || strncmp(spec, "postgresql://", 13) == 0)
        name = "postgres", location = spec;

    if (!location || !*location)
    {
        fprintf(stderr, "BB_TODO_DB: expected sqlite:<path>, json:<path> or postgresql://...\n");
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
    const bb_model_api_t *api;
    bb_model_handle_t *handle;

    if (open_backend(getenv("BB_TODO_DB"), &api, &handle) != 0)
        return 1;

    /* init repo */
    bb_repo_init(&global_task_repo.base, api, handle, &Task_schema);

    bb_server_t *server = bb_server_create(8080);

    bb_server_use_pre_middleware(server, server_header_middleware);
    bb_server_use_post_middleware(server, logger_middleware);

    bb_server_add_route(server, "GET", "/", root);
    bb_server_add_route(server, "POST", "/add_task", add_task);
    bb_server_add_route(server, "POST", "/remove_task/:id", remove_task);
    bb_server_add_route(server, "POST", "/mark_done/:id", mark_done);
    bb_server_add_route(server, "GET", "/:id/status", get_task);
    bb_server_add_route(server, "GET", "/list_tasks", list_tasks);
 
    bb_server_start(server);
    bb_runtime_run_default();
    return 0;
}
