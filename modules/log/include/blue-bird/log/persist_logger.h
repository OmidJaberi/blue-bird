#ifndef BB_PERSIST_LOGGER_H
#define BB_PERSIST_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/log/log.h"

void bb_logger_init_persist(bb_logger_t *logger, bb_log_level_t level);
void bb_logger_free_persist_context(bb_logger_t *logger);


#ifdef __cplusplus
}
#endif

#endif //BB_PERSIST_LOGGER_H
