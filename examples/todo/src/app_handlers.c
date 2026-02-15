#include "app_handlers.h"
#include "log/log.h"

#include "persist/persist.h"
#include "persist/persist_sqlite.h"

#include <stdlib.h>
#include <string.h>

BBError add_task(request_t *req, response_t *res)
{
    http_message_t *msg = &GET_SERVER_REQUEST_MESSAGE(*req);
    LOG_INFO("Add task: %s\n", msg->body);
    char *task_key = malloc((strlen(msg->body) + 5) * sizeof(char));
    if (!task_key)
    {
        printf("FAIL: malloc\n");
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    sprintf(task_key, "task:%s", msg->body);
    if (persist_save(task_key, "not_done", 8) != 0)
    {
        printf("FAIL: persist_save\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Failed to save.");
    }
    free(task_key);
    return BB_SUCCESS();
}

BBError mark_done(request_t *req, response_t *res)
{
    const char *task_name = get_request_param(req, "task_name");
    LOG_INFO("Mark done: %s\n", task_name);
    char *task_key = malloc((strlen(task_name) + 5) * sizeof(char));
    if (!task_key)
    {
        printf("FAIL: malloc\n");
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    sprintf(task_key, "task:%s", task_name);
    char buf[64] = {0};
    if (persist_load(task_key, buf, sizeof(buf)) != 0)
    {
        printf("FAIL: persist_load\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "FAIL: persist_load.");
    }
    if (persist_save(task_key, "done", 8) != 0)
    {
        printf("FAIL: persist_save\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Failed to save.");
    }
    free(task_key);
    return BB_SUCCESS();
}

BBError get_task(request_t *req, response_t *res)
{
    const char *task_name = get_request_param(req, "task_name");
    LOG_INFO("Get task: %s\n", task_name);
    char *task_key = malloc((strlen(task_name) + 5) * sizeof(char));
    if (!task_key)
    {
        printf("FAIL: malloc\n");
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    sprintf(task_key, "task:%s", task_name);
    char buf[64] = {0};
    if (persist_load(task_key, buf, sizeof(buf)) != 0)
    {
        printf("FAIL: persist_load\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "FAIL: persist_load.");
    }
    set_response_body(res, buf);
    free(task_key);
    return BB_SUCCESS();
}

BBError list_tasks(request_t *req, response_t *res)
{
    return BB_SUCCESS();
}
