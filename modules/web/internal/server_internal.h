#ifndef BB_SERVER_INTERNAL_H
#define BB_SERVER_INTERNAL_H

#include "blue-bird/web/server.h"
#include "websocket/session.h"

bb_error_t bb_server_run_request_pipeline(bb_server_t *server, bb_ws_session_t **session, bb_request_t *req, bb_response_t *res);

#endif //BB_SERVER_INTERNAL_H
