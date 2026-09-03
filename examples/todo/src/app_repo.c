#include "app_repo.h"

#include <blue-bird/persist/serialization/entity_json.h>
#include <blue-bird/utils/json.h>

TaskRepo global_task_repo;

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
    bb_json_t *json = bb_entity_to_json(&Task_schema, task);
    int res = BB_FAILED(bb_json_serialize(json, s, size)) ? 1 : 0;
    bb_json_destroy(json);
    return res;
}
