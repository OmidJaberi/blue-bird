#ifndef BB_RUNTIME_RUNTIME_H
#define BB_RUNTIME_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/runtime/loop.h"

typedef struct bb_runtime bb_runtime_t;

bb_runtime_t *bb_runtime_create(void);

void bb_runtime_destroy(
    bb_runtime_t *runtime
);

void bb_runtime_run(
    bb_runtime_t *runtime
);

bb_loop_t *bb_runtime_loop(
    bb_runtime_t *runtime
);


#ifdef __cplusplus
}
#endif

#endif
