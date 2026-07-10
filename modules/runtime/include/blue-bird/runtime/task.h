#ifndef BB_RUNTIME_TASK_H
#define BB_RUNTIME_TASK_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct bb_task bb_task_t;

typedef void (*bb_task_cb)(bb_task_t *task, void *userdata);


bb_task_t *bb_task_create(bb_task_cb callback, void *userdata);

void bb_task_destroy(bb_task_t *task);

int bb_task_cancel(bb_task_t *task);

int bb_task_is_cancelled(const bb_task_t *task);

int bb_task_is_scheduled(const bb_task_t *task);


#ifdef __cplusplus
}
#endif

#endif
