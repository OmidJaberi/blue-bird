#include "app_repo.h"

#include "persist/entity_json.h"
#include "utils/json.h"

TaskRepo global_task_repo;

BB_Field task_fields[] = {
    { "id", BB_FIELD_UUID, offsetof(Task, id), BB_UUID_BUF_LEN },
    { "name", BB_FIELD_STRING, offsetof(Task, name), 64 },
    { "status", BB_FIELD_STRING, offsetof(Task, status), 64 }
};

BB_Schema task_schema = {
    .name = "tasks",
    .fields = task_fields,
    .field_count = 3,
    .struct_size = sizeof(Task),
    .primary_key_index = 0
};

int task_insert(TaskRepo *repo, Task *task)
{
    return bb_repo_insert(&repo->base, task);
}

int task_remove(TaskRepo *repo, const char *id)
{
    return bb_repo_remove(&repo->base, id);
}

int task_update(TaskRepo *repo, Task *task)
{
    return bb_repo_update(&repo->base, task);
}

int serialize_task(Task *task, char **s, int *size)
{
    json_node_t *json = bb_entity_to_json(&task_schema, task);
    int res = serialize_json(json, s, size);
    destroy_json(json);
    free(json);
    return res;
}