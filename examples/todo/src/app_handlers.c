#include "app_handlers.h"
#include "log/log.h"

#include "persist/persist.h"
#include "persist/persist_sqlite.h"

BBError add_task(request_t *req, response_t *res)
{
    http_message_t *msg = &GET_SERVER_REQUEST_MESSAGE(*req);
    LOG_INFO("Add task: %s\n", msg->body);
    if (persist_save(msg->body, "not_done", 8) != 0) {
        printf("FAIL: persist_save\n");
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Failed to save.");
    }
    return BB_SUCCESS();
}

BBError list_tasks(request_t *req, response_t *res)
{
    return BB_SUCCESS();
}
