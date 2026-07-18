#include "app_repo.h"

#include <blue-bird/persist/serialization/entity_json.h>
#include <blue-bird/utils/json.h>

TaskRepo global_task_repo;

bb_field_t task_fields[] = {
    {
        .name = "id",
        .type = BB_FIELD_UUID,
        .offset = offsetof(Task, id),
        .size = BB_UUID_BUF_LEN,
        .flags = BB_FIELD_NONE
    },
    {
        .name = "name",
        .type = BB_FIELD_STRING,
        .offset = offsetof(Task, name),
        .size = 64,
        .flags = BB_FIELD_NONE
    },
    {
        .name = "status",
        .type = BB_FIELD_STRING,
        .offset = offsetof(Task, status),
        .size = 64,
        .flags = BB_FIELD_NONE
    }
};

bb_schema_t task_schema = {
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
    bb_json_t *json = bb_entity_to_json(&task_schema, task);
    int res = BB_FAILED(bb_json_serialize(json, s, size)) ? 1 : 0;
    bb_json_destroy(json);
    return res;
}
