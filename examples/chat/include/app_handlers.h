#ifndef APP_HANDLERS_H
#define APP_HANDLERS_H

#include <blue-bird/web/http/handler.h>

bb_error_t serve_index(bb_request_t *req, bb_response_t *res);

bb_error_t api_register(bb_request_t *req, bb_response_t *res);
bb_error_t api_login(bb_request_t *req, bb_response_t *res);
bb_error_t api_logout(bb_request_t *req, bb_response_t *res);
bb_error_t api_me(bb_request_t *req, bb_response_t *res);
bb_error_t api_list_users(bb_request_t *req, bb_response_t *res);
bb_error_t api_conversation(bb_request_t *req, bb_response_t *res);

#endif //APP_HANDLERS_H
