#ifndef APP_REPO_H
#define APP_REPO_H

#include "Task_schema.generated.h"

#include <blue-bird/persist/model.h>
#include <blue-bird/persist/repo.h>

typedef struct {
    bb_repo_t base;
} TaskRepo;

extern TaskRepo global_task_repo;

int task_insert(TaskRepo *repo, Task *task);
int task_remove(TaskRepo *repo, const char *id);
int task_update(TaskRepo *repo, Task *task);

int serialize_task(Task *task, char **s, int *size);

#endif //APP_REPO_H
