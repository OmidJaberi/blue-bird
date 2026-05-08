#include "app_handlers.h"
#include "app_middleware.h"
#include "app_repo.h"

#include "blue-bird/web/http.h"
#include "blue-bird/web/server.h"
#include "blue-bird/log/console_logger.h"
#include "blue-bird/persist/model/model_sqlite.h"

int main()
{
    Logger console_logger;
    logger_init_console(&console_logger, LOG_LEVEL_INFO, stderr);
    default_logger = console_logger;

    const char *dbfile = "todo_sqlite.db";

    /* Register repo backend */
    bb_model_register(bb_model_sqlite_api());
    const BB_ModelAPI *api = bb_model_get("sqlite");
    BB_ModelHandle *handle = api->open(dbfile);

    /* init repo */
    bb_repo_init(&global_task_repo.base, api, handle, &task_schema);

    bb_server_t server;
    init_server(&server, 8080);

    use_pre_middleware(&server, server_header_middleware);
    use_post_middleware(&server, logger_middleware);

    add_route(&server, "POST", "/add_task", add_task);
    add_route(&server, "DELETE", "/remove_task/:id", remove_task);
    add_route(&server, "POST", "/mark_done/:id", mark_done);
    add_route(&server, "GET", "/:id/status", get_task);
    add_route(&server, "GET", "/list_tasks", list_tasks);
 
    start_server(&server);
    return 0;
}
