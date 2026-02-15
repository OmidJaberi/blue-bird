#ifndef APP_HANDLERS_H
#define APP_HANDLERS_H

#include "core/http.h"

BBError add_task(request_t *req, response_t *res);
BBError remove_task(request_t *req, response_t *res);
BBError get_task(request_t *req, response_t *res);
BBError mark_done(request_t *req, response_t *res);
BBError list_tasks(request_t *req, response_t *res);

#endif // APP_HANDLERS_H
