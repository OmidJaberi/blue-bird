#ifndef APP_HANDLERS_H
#define APP_HANDLERS_H

#include <blue-bird/web/http.h>

bb_error_t add_task(request_t *req, response_t *res);
bb_error_t remove_task(request_t *req, response_t *res);
bb_error_t get_task(request_t *req, response_t *res);
bb_error_t mark_done(request_t *req, response_t *res);
bb_error_t list_tasks(request_t *req, response_t *res);

#endif // APP_HANDLERS_H
