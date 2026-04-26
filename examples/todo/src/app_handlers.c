#include "app_handlers.h"
#include "app_repo.h"
#include "log/log.h"

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

    if (bb_repo_find_by_id(&global_task_repo.base, &t, id) != 0)
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

    if (bb_repo_find_by_id(&global_task_repo.base, &t, id) != 0)
    {
        set_response_status(res, 404);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Not found");
    }

    set_response_status(res, 200);
    set_response_body(res, t.status);

    return BB_SUCCESS();
}

BBError list_tasks(request_t *req, response_t *res)
{
    (void)req;

    set_response_status(res, 501);
    set_response_body(res, "Not implemented");

    return BB_SUCCESS();
}
