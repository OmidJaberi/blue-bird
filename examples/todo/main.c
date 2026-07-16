#include "app_handlers.h"
#include "app_middleware.h"
#include "app_repo.h"

#include <blue-bird/runtime/runtime.h>
#include <blue-bird/web/server.h>
#include <blue-bird/log/console_logger.h>
#include <blue-bird/persist/model/model_sqlite.h>

int main(void)
{
    const char *dbfile = "todo_sqlite.db";

    /* Register repo backend */
    bb_model_register(bb_model_sqlite_api());
    const bb_model_api_t *api = bb_model_get("sqlite");
    bb_model_handle_t *handle = api->open(dbfile);

    /* init repo */
    bb_repo_init(&global_task_repo.base, api, handle, &task_schema);

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
