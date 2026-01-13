#include "app_handlers.h"

#include "core/http.h"
#include "core/server.h"
#include "log/console_logger.h"
#include "persist/persist.h"
#include "persist/persist_sqlite.h"

int main()
{
    Logger console_logger;
    logger_init_console(&console_logger, LOG_LEVEL_INFO, stderr);
    default_logger = console_logger;

    const char *dbfile = "todo_sqlite.db";

    /* Clean up from previous runs */
    // unlink(dbfile);

    /* Register backend */
    persist_sqlite_register();
    persist_set_default("sqlite");
    persist_set_default_uri(dbfile);

    Server server;
    init_server(&server, 8080);

    add_route(&server, "POST", "/add_task", add_task);
    add_route(&server, "GET", "/list_tasks", list_tasks);
    
    start_server(&server);
    return 0;
}
