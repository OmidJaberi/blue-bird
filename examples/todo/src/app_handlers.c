#include "app_handlers.h"
#include "app_repo.h"
#include "log/log.h"

#include "persist/key_val.h"
#include "persist/key_val/persist_sqlite.h"
#include "utils/json.h"

#include <stdlib.h>
#include <string.h>

// Repo Logic
json_node_t *get_list()
{
    char *buf = (char*)malloc(sizeof(char) * 1000);
    if (persist_load("task_list", buf, sizeof(buf)) == 0)
    {
        json_node_t *arr = JSON_NEW(JSON_ARRAY);
        if (parse_json_str(arr, buf) != -1)
        {
            free(buf);
            return arr;
        }
        destroy_json(arr);
        free(arr);
    }
    free(buf);
    return JSON_NEW(JSON_ARRAY);
}

BBError add_to_list(const char *task_name)
{
    json_node_t *arr = get_list();
    push_json_array(arr, JSON_NEW_TEXT(task_name));
    char *arr_str;
    int size;
    serialize_json(arr, &arr_str, &size);
    if (persist_save("task_list", arr_str, size) != 0)
    {
        LOG_ERROR("FAIL: persist_save\n");
        free(arr_str);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to save.");
    }
    destroy_json(arr);
    free(arr);
    free(arr_str);
    return BB_SUCCESS();
}

BBError delete_from_list(const char *task_name)
{
    json_node_t *arr = get_list();
    for (int i = 0; i < arr->size; i++)
    {
        if (strcmp(get_json_text_value(get_json_array_index(arr, i)), task_name) == 0)
        {
            json_array_remove_at_index(arr, i);
            break;
        }
    }
    char *arr_str;
    int size;
    int err = serialize_json(arr, &arr_str, &size);
    destroy_json(arr);
    free(arr);
    if (err != 0)
    {
        LOG_ERROR("FAIL: persist_save serialization\n");
        return BB_ERROR(BB_ERR_ALLOC, "Failed to serialize.");
    }
    err = persist_save("task_list", arr_str, size);
    free(arr_str);
    if (err != 0)
    {
        LOG_ERROR("FAIL: persist_save\n");
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Failed to save.");
    }
    return BB_SUCCESS();
}

BBError get_task_key(const char *task_name, char **buffer)
{
    *buffer = malloc((strlen(task_name) + 5) * sizeof(char));
    if (!*buffer)
    {
        LOG_ERROR("FAIL: malloc\n");
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    sprintf(*buffer, "task:%s", task_name);
    return BB_SUCCESS();
}

BBError is_task_done(const char *task_name, int *is_done)
{
    char *task_key;
    if (BB_FAILED(get_task_key(task_name, &task_key)))
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    char buf[64] = {0};
    if (persist_load(task_key, buf, sizeof(buf)) != 0)
    {
        LOG_ERROR("FAIL: persist_load\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "FAIL: persist_load.");
    }
    free(task_key);
    *is_done = strcmp(buf, "done");
    return BB_SUCCESS();
}

BBError mark_task_done(const char *task_name)
{
    char *task_key;
    if (BB_FAILED(get_task_key(task_name, &task_key)))
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    char buf[64] = {0};
    if (persist_load(task_key, buf, sizeof(buf)) != 0)
    {
        LOG_ERROR("FAIL: persist_load\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "FAIL: persist_load.");
    }
    if (persist_save(task_key, "done", 8) != 0)
    {
        LOG_ERROR("FAIL: persist_save\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Failed to save.");
    }
    free(task_key);
    return BB_SUCCESS();
}

BBError add_new_task(const char *task_name)
{
    char *task_key;
    if (BB_FAILED(get_task_key(task_name, &task_key)))
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    char buf[64] = {0};
    if (persist_load(task_key, buf, sizeof(buf)) == 0)
    {
        LOG_INFO("FAIL: existing_task\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "FAIL: existing_task.");
    }
    if (persist_save(task_key, "not_done", 8) != 0)
    {
        LOG_ERROR("FAIL: persist_save\n");
        free(task_key);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to save.");
    }
    free(task_key);
    add_to_list(task_name);
    return BB_SUCCESS();
}

BBError delete_task(const char *task_name)
{
    char *task_key;
    if (BB_FAILED(get_task_key(task_name, &task_key)))
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    if (persist_remove(task_key) != 0)
    {
        LOG_ERROR("FAIL: persist_remove\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Failed to remove.");
    }
    free(task_key);
    delete_from_list(task_name);
    return BB_SUCCESS();
}

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
    json_node_t *arr = get_list();
    char *buf;
    int size;
    if (serialize_json(arr, &buf, &size) != 0)
    {
        set_response_status(res, 500);
        return BB_ERROR(BB_ERR_ALLOC, "Failed to serialize.");
    }
    set_response_body(res, buf);
    return BB_SUCCESS();
}
