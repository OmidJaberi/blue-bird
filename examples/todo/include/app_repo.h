#ifndef APP_REPO_H
#define APP_REPO_H

#include <blue-bird/persist/model.h>
#include <blue-bird/persist/repo.h>
#include <blue-bird/utils/uuid.h>

#include <stddef.h>

typedef struct {
    bb_uuid_t id;
    char name[64];
    char status[64];
} Task;

typedef struct {
    bb_repo_t base;
} TaskRepo;

extern TaskRepo global_task_repo;

extern bb_field_t task_fields[];
extern bb_schema_t task_schema;

int task_insert(TaskRepo *repo, Task *task);
int task_remove(TaskRepo *repo, const char *id);
int task_update(TaskRepo *repo, Task *task);

int serialize_task(Task *task, char **s, int *size);

#endif //APP_REPO_H
