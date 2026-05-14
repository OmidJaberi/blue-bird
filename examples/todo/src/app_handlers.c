#include "app_handlers.h"
#include "app_repo.h"

#include <blue-bird/log/log.h>
#include <blue-bird/utils/json.h>
#include <blue-bird/utils/uuid.h>

#include <stdlib.h>
#include <string.h>

// Route Handlers:
bb_error_t add_task(bb_request_t *req, bb_response_t *res)
{
    bb_http_message_t *msg = &BB_REQUEST_GET_MESSAGE(*req);

    Task t = {0};
    bb_uuid_v4_string(t.id);
    strncpy(t.name, msg->body, sizeof(t.name));
    strcpy(t.status, "not_done");

    int rc = task_insert(&global_task_repo, &t);

    if (rc == 0)
    {
        char buff[256];
        snprintf(buff, 256, "id: %s", t.id);
        bb_response_set_body(res, buff);
        bb_response_set_status(res, 200);
        return BB_SUCCESS();
    }

    bb_response_set_status(res, 500);
    return BB_ERROR(BB_ERR_INTERNAL, "Insert failed");
}

bb_error_t remove_task(bb_request_t *req, bb_response_t *res)
{
    const char *id = bb_request_get_param(req, "id");

    int rc = task_remove(&global_task_repo, id);

    if (rc == 0)
    {
        bb_response_set_status(res, 200);
        return BB_SUCCESS();
    }

    bb_response_set_status(res, 404);
    return BB_ERROR(BB_ERR_BAD_REQUEST, "Not found");
}

bb_error_t mark_done(bb_request_t *req, bb_response_t *res)
{
    const char *id = bb_request_get_param(req, "id");

    Task t = {0};

    if (bb_repo_find_by_pk(&global_task_repo.base, &t, id) != 0)
    {
        bb_response_set_status(res, 404);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Not found");
    }

    strcpy(t.status, "done");

    if (task_update(&global_task_repo, &t) != 0)
    {
        bb_response_set_status(res, 500);
        return BB_ERROR(BB_ERR_INTERNAL, "Update failed");
    }

    bb_response_set_status(res, 200);
    return BB_SUCCESS();
}

bb_error_t get_task(bb_request_t *req, bb_response_t *res)
{
    const char *id = bb_request_get_param(req, "id");

    Task t = {0};

    if (bb_repo_find_by_pk(&global_task_repo.base, &t, id) != 0)
    {
        bb_response_set_status(res, 404);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Not found");
    }

    char *buf;
    int size;
    int rc = serialize_task(&t, &buf, &size);
    if (rc != 0)
    {
        bb_response_set_status(res, 500);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to serialize.");
    }

    bb_response_set_status(res, 200);
    bb_response_set_body(res, buf);
    free(buf);

    return BB_SUCCESS();
}

bb_error_t list_tasks(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    Task *tasks = NULL;
    size_t count = 0;

    bb_repo_find_all(&global_task_repo.base, (void**)&tasks, &count);

    bb_json_t task_list;
    bb_json_init(&task_list, BB_JSON_ARRAY);
    for (unsigned long i = 0; i < count; i++)
    {
        bb_json_t *task = BB_JSON(
            OBJ(
                KEY("id", TEXT(tasks[i].id)),
                KEY("name", TEXT(tasks[i].name)),
                KEY("status", TEXT(tasks[i].status))
            )
        );
        bb_json_array_push(&task_list, task);
    }

    
    char *buf;
    int size;
    bb_json_serialize(&task_list, &buf, &size);
    bb_json_destroy(&task_list);
    if (size <= 0)
    {
        bb_response_set_status(res, 500);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to serialize.");
    }

    bb_response_set_status(res, 200);
    bb_response_set_body(res, buf);

    free(tasks);

    return BB_SUCCESS();
}
