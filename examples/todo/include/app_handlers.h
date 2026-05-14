#ifndef APP_HANDLERS_H
#define APP_HANDLERS_H

#include <blue-bird/web/http.h>

bb_error_t add_task(bb_request_t *req, bb_response_t *res);
bb_error_t remove_task(bb_request_t *req, bb_response_t *res);
bb_error_t get_task(bb_request_t *req, bb_response_t *res);
bb_error_t mark_done(bb_request_t *req, bb_response_t *res);
bb_error_t list_tasks(bb_request_t *req, bb_response_t *res);

#endif // APP_HANDLERS_H
