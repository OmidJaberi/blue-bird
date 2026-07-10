#ifndef BB_RUNTIME_TASK_H
#define BB_RUNTIME_TASK_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct bb_task bb_task_t;

typedef void (*bb_task_cb)(bb_task_t *task, void *userdata);


int bb_task_is_cancelled(const bb_task_t *task);

int bb_task_is_scheduled(const bb_task_t *task);


#ifdef __cplusplus
}
#endif

#endif
