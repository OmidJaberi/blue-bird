#include "app_repo.h"

BB_Field task_fields[] = {
    { "id", BB_FIELD_INT, offsetof(Task, id), sizeof(int) },
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
