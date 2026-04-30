#include "app_handlers.h"
#include "app_repo.h"
#include "log/log.h"
#include "utils/json.h"

#include <stdlib.h>
#include <string.h>

// Route Handlers:
BBError add_task(request_t *req, response_t *res)
{
    http_message_t *msg = &GET_REQUEST_MESSAGE(*req);

    Task t = {0};
    t.id = rand(); // for now...
    strncpy(t.name, msg->body, sizeof(t.name));
    strcpy(t.status, "not_done");

    int rc = task_insert(&global_task_repo, &t);

    if (rc == 0)
    {
        char buff[256];
        snprintf(buff, 256, "id: %d", t.id);
        set_response_body(res, buff);
        set_response_status(res, 200);
        return BB_SUCCESS();
    }

    set_response_status(res, 500);
    return BB_ERROR(BB_ERR_INTERNAL, "Insert failed");
}

BBError remove_task(request_t *req, response_t *res)
{
    const char *id_str = get_request_param(req, "id");
    int id = atoi(id_str);

    int rc = task_remove(&global_task_repo, id);

    if (rc == 0)
    {
        set_response_status(res, 200);
        return BB_SUCCESS();
    }

    set_response_status(res, 404);
    return BB_ERROR(BB_ERR_BAD_REQUEST, "Not found");
}

BBError mark_done(request_t *req, response_t *res)
{
    const char *id_str = get_request_param(req, "id");
    int id = atoi(id_str);

    Task t = {0};

    if (bb_repo_find_by_pk(&global_task_repo.base, &t, &id) != 0)
    {
        set_response_status(res, 404);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Not found");
    }

    strcpy(t.status, "done");

    if (task_update(&global_task_repo, &t) != 0)
    {
        set_response_status(res, 500);
        return BB_ERROR(BB_ERR_INTERNAL, "Update failed");
    }

    set_response_status(res, 200);
    return BB_SUCCESS();
}

BBError get_task(request_t *req, response_t *res)
{
    const char *id_str = get_request_param(req, "id");
    int id = atoi(id_str);

    Task t = {0};

    if (bb_repo_find_by_pk(&global_task_repo.base, &t, &id) != 0)
    {
        set_response_status(res, 404);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Not found");
    }

    json_node_t *doc = JSON(
        OBJ(
            KEY("id", INT(t.id)),
            KEY("name", TEXT(t.name)),
            KEY("status", TEXT(t.status))
        )
    );
    char *buf;
    int size;
    serialize_json(doc, &buf, &size);
    destroy_json(doc);
    free(doc);
    if (size <= 0)
    {
        set_response_status(res, 500);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to serialize.");
    }

    set_response_status(res, 200);
    set_response_body(res, buf);
    free(buf);

    return BB_SUCCESS();
}

BBError list_tasks(request_t *req, response_t *res)
{
    Task *tasks = NULL;
    size_t count = 0;

    bb_repo_find_all(&global_task_repo.base, (void**)&tasks, &count);

    json_node_t task_list;
    init_json(&task_list, JSON_ARRAY);
    for (int i = 0; i < count; i++)
    {
        json_node_t *task = JSON(
            OBJ(
                KEY("id", INT(tasks[i].id)),
                KEY("name", TEXT(tasks[i].name)),
                KEY("status", TEXT(tasks[i].status))
            )
        );
        push_json_array(&task_list, task);
    }

    
    char *buf;
    int size;
    serialize_json(&task_list, &buf, &size);
    destroy_json(&task_list);
    if (size <= 0)
    {
        set_response_status(res, 500);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to serialize.");
    }

    set_response_status(res, 200);
    set_response_body(res, buf);

    free(tasks);

    return BB_SUCCESS();
}
