#ifndef BB_RUNTIME_TASK_H
#define BB_RUNTIME_TASK_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct bb_task bb_task_t;

typedef void (*bb_task_cb)(
    bb_task_t *task,
    void *userdata
);

struct bb_task {
    bb_task_cb callback;
    void *userdata;
};

bb_task_t *bb_task_create(bb_task_cb callback, void *userdata);

void bb_task_destroy(bb_task_t *task);

void bb_task_execute(bb_task_t *task);


#ifdef __cplusplus
}
#endif

#endif
