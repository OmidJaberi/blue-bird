#include "app_handlers.h"
#include "log/log.h"

#include "persist/persist.h"
#include "persist/persist_sqlite.h"
#include "utils/json.h"

#include <stdlib.h>
#include <string.h>

// Repo Logic
BBError get_task_key(const char *task_name, char *buffer)
{
    buffer = malloc((strlen(task_name) + 5) * sizeof(char));
    if (!buffer)
    {
        printf("FAIL: malloc\n");
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    sprintf(buffer, "task:%s", task_name);
    return BB_SUCCESS();
}

BBError is_task_done(const char *task_name, int *is_done)
{
    char *task_key;
    get_task_key(task_name, task_key);
    if (BB_FAILED(get_task_key(task_name, task_key)))
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    char buf[64] = {0};
    if (persist_load(task_key, buf, sizeof(buf)) != 0)
    {
        printf("FAIL: persist_load\n");
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
    get_task_key(task_name, task_key);
    if (BB_FAILED(get_task_key(task_name, task_key)))
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
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

BBError add_new_task(const char *task_name)
{
    char *task_key;
    get_task_key(task_name, task_key);
    if (BB_FAILED(get_task_key(task_name, task_key)))
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    char buf[64] = {0};
    if (persist_load(task_key, buf, sizeof(buf)) == 0)
    {
        printf("FAIL: existing_task\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "FAIL: existing_task.");
    }
    if (persist_save(task_key, "not_done", 8) != 0)
    {
        printf("FAIL: persist_save\n");
        free(task_key);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to save.");
    }
    free(task_key);

    return BB_SUCCESS();
}

BBError delete_task(const char *task_name)
{
    char *task_key;
    get_task_key(task_name, task_key);
    if (BB_FAILED(get_task_key(task_name, task_key)))
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to malloc.");
    }
    if (persist_remove(task_key) != 0)
    {
        printf("FAIL: persist_remove\n");
        free(task_key);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Failed to remove.");
    }
    free(task_key);
    return BB_SUCCESS();
}

// Route Handlers:
BBError add_task(request_t *req, response_t *res)
{
    http_message_t *msg = &GET_SERVER_REQUEST_MESSAGE(*req);
    LOG_INFO("Add task: %s\n", msg->body);
    BBError err = add_new_task(msg->body);
    switch (err.code)
    {
        case BB_OK:
            // Add to list:
            char buf[100000] = {0};
            json_node_t *arr = JSON_NEW(JSON_ARRAY);
            if (persist_load("task_list", buf, sizeof(buf)) == 0)
            {
                parse_json_str(arr, buf);
            }
            push_json_array(arr, JSON_NEW_TEXT(msg->body));
            char *arr_str;
            int size;
            serialize_json(arr, &arr_str, &size);
            if (persist_save("task_list", arr_str, size) != 0)
            {
                printf("FAIL: persist_save\n");
                free(arr_str);
                set_response_status(res, 500);
                return BB_ERROR(BB_ERR_BAD_REQUEST, "Failed to save.");
            }
            destroy_json(arr);
            free(arr);
            free(arr_str);
            break;
        case BB_ERR_BAD_REQUEST:
            set_response_status(res, 409);
            break;
        default:
            set_response_status(res, 500);
            break;
    }
    return err;
}

BBError remove_task(request_t *req, response_t *res)
{
    const char *task_name = get_request_param(req, "task_name");
    LOG_INFO("Remove task: %s\n", task_name);
    BBError err = delete_task(task_name);
    switch (err.code)
    {
        case BB_OK:
            // Remove from list:
            char buf[100000] = {0};
            json_node_t *arr = JSON_NEW(JSON_ARRAY);
            json_node_t *new_arr = JSON_NEW(JSON_ARRAY);
            if (persist_load("task_list", buf, sizeof(buf)) == 0)
            {
                parse_json_str(arr, buf);
            }
            for (int i = 0; i < arr->size; i++)
            {
                char *val = get_json_array_index(arr, i)->value.text_val;
                if (strcmp(val, task_name) != 0)
                {
                    push_json_array(new_arr, JSON_NEW_TEXT(val));
                }
            }
            destroy_json(arr);
            free(arr);
            char *arr_str;
            int size;
            serialize_json(new_arr, &arr_str, &size);
            if (persist_save("task_list", arr_str, size) != 0)
            {
                printf("FAIL: persist_save\n");
                free(arr_str);
                set_response_status(res, 500);
                return BB_ERROR(BB_ERR_BAD_REQUEST, "Failed to save.");
            }
            destroy_json(new_arr);
            free(new_arr);
            free(arr_str);            
            break;
        default:
            set_response_status(res, 500);
            break;
    }
    return err;
}

BBError mark_done(request_t *req, response_t *res)
{
    const char *task_name = get_request_param(req, "task_name");
    LOG_INFO("Mark done: %s\n", task_name);
    BBError err = mark_task_done(task_name);
    switch (err.code)
    {
        case BB_OK:
            set_response_status(res, 200);
            break;
        case BB_ERR_BAD_REQUEST:
            set_response_status(res, 404);
            break;
        default:
            set_response_status(res, 500);
            break;
    }
    return err;
}

BBError get_task(request_t *req, response_t *res)
{
    const char *task_name = get_request_param(req, "task_name");
    LOG_INFO("Get task: %s\n", task_name);
    int is_done;
    BBError err = is_task_done(task_name, &is_done);
    switch (err.code)
    {
        case BB_OK:
            set_response_status(res, 200);
            set_response_body(res, is_done ? "done" : "not_done");
            break;
        case BB_ERR_BAD_REQUEST:
            set_response_status(res, 404);
            break;
        default:
            set_response_status(res, 500);
            break;
    }
    return err;
}

BBError list_tasks(request_t *req, response_t *res)
{
    char buf[100000] = {0};
    if (persist_load("task_list", buf, sizeof(buf)) != 0)
    {
        printf("FAIL: persist_load\n");
        set_response_status(res, 404);
        return BB_ERROR(BB_ERR_BAD_REQUEST, "FAIL: persist_load.");
    }
    set_response_body(res, buf);
    return BB_SUCCESS();
}
