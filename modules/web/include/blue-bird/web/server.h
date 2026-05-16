#ifndef BB_SERVER_H
#define BB_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/web/router.h"
#include "blue-bird/web/middleware.h"
#include "blue-bird/web/executor.h"

typedef struct {
    int server_fd;
    bb_web_executor_t *executor;
    bb_route_list_t *route_list;
    bb_middleware_list_t *pre_middleware_list; // Runs before the handler
    bb_middleware_list_t *post_middleware_list; // Runs after the handler
} bb_server_t;

int bb_server_init(bb_server_t *server, int port);
void bb_server_add_route(bb_server_t *server, const char *method, const char *path, bb_route_handler_cb handler);
void bb_server_use_pre_middleware(bb_server_t *server, bb_middleware_cb mw);
void bb_server_use_post_middleware(bb_server_t *server, bb_middleware_cb mw);
void bb_server_start(bb_server_t *server);
void bb_server_destroy(bb_server_t *server);


#ifdef __cplusplus
}
#endif

#endif //BB_SERVER_H
