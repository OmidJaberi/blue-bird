#include "app_handlers.h"
#include "app_ws.h"
#include "app_repo.h"

#include <blue-bird/runtime/runtime.h>
#include <blue-bird/web/server.h>
#include <blue-bird/log/console_logger.h>
#include <blue-bird/persist/model/model_sqlite.h>

int main(void)
{
    const char *dbfile = "chat_sqlite.db";

    /* Register the sqlite persistence backend and open the database. */
    bb_model_register(bb_model_sqlite_api());
    const bb_model_api_t *api = bb_model_get("sqlite");
    bb_model_handle_t *handle = api->open(dbfile);

    /* Both repos share the same sqlite connection/handle. */
    bb_repo_init(&global_user_repo.base, api, handle, &user_schema);
    bb_repo_init(&global_message_repo.base, api, handle, &message_schema);

    bb_server_t *server = bb_server_create(8080);

    /* Frontend (single page, embeds both the auth screen and chat UI) */
    bb_server_add_route(server, "GET", "/", serve_index);

    /* Auth API */
    bb_server_add_route(server, "POST", "/api/register", api_register);
    bb_server_add_route(server, "POST", "/api/login", api_login);
    bb_server_add_route(server, "POST", "/api/logout", api_logout);
    bb_server_add_route(server, "GET", "/api/me", api_me);

    /* Contacts + history */
    bb_server_add_route(server, "GET", "/api/users", api_list_users);
    bb_server_add_route(server, "GET", "/api/messages/:peer", api_conversation);

    /* Live chat */
    bb_server_add_websocket(server, "/ws", chat_ws_handler);

    bb_server_start(server);

    BB_LOG_INFO("Blue-Bird chat example running on http://localhost:8080\n");

    bb_runtime_run_default();
    return 0;
}
