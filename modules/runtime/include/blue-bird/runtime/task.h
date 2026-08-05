#ifndef BB_RUNTIME_TASK_H
#define BB_RUNTIME_TASK_H

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    BB_TASK_COMPLETED,
    BB_TASK_ERROR,
    BB_TASK_CANCELLED,
    BB_TASK_SHUTDOWN,
} bb_task_result_t;

typedef struct bb_task bb_task_t;

typedef void (*bb_task_cb)(bb_task_t *task, void *userdata);
typedef void (*bb_task_cleanup_cb)(bb_task_t *task, void *userdata, bb_task_result_t result);

typedef struct
{
    bb_task_cb run;
    void *userdata;
    bb_task_cleanup_cb cleanup;
} bb_task_config_t;

int bb_task_is_cancelled(const bb_task_t *task);

int bb_task_is_scheduled(const bb_task_t *task);


#ifdef __cplusplus
}
#endif

#endif
