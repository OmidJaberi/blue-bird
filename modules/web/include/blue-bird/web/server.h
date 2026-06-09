#ifndef BB_SERVER_H
#define BB_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/runtime/runtime.h"

#include "http/handler.h"

typedef struct bb_server bb_server_t;

bb_server_t *bb_server_create_on_runtime(bb_runtime_t *runtime, int port);
void bb_server_add_route(bb_server_t *server, const char *method, const char *path, bb_http_handler_cb handler);
void bb_server_use_pre_middleware(bb_server_t *server, bb_http_handler_cb mw);
void bb_server_use_post_middleware(bb_server_t *server, bb_http_handler_cb mw);
void bb_server_start(bb_server_t *server);
void bb_server_destroy(bb_server_t *server);

static inline bb_server_t *bb_server_create(int port)
{
    return bb_server_create_on_runtime(bb_runtime_default(), port);
}


#ifdef __cplusplus
}
#endif

#endif //BB_SERVER_H
