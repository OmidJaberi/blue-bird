#include "app_handlers.h"

#include "core/http.h"
#include "core/server.h"
#include "log/console_logger.h"

int main()
{
    Logger console_logger;
    logger_init_console(&console_logger, LOG_LEVEL_INFO, stderr);
    default_logger = console_logger;

    Server server;
    init_server(&server, 8080);

    add_route(&server, "POST", "/:id/add_task", add_task);
    add_route(&server, "GET", "/:id/list_tasks", list_tasks);
    
    start_server(&server);
    return 0;
}
