#ifndef APP_REPO_H
#define APP_REPO_H

#include "persist/model.h"
#include "repo/repo.h"
#include <stddef.h>

typedef struct {
    int id;
    char name[64];
    char status[64];
} Task;

typedef struct {
    BB_Repo base;
} TaskRepo;

extern BB_Field task_fields[];
extern BB_Schema task_schema;

int task_insert(TaskRepo *repo, Task *task);
int task_remove(TaskRepo *repo, int id);
int task_update(TaskRepo *repo, Task *task);

#endif //APP_REPO_H
