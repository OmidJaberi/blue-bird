#ifndef BB_PERSIST_LOGGER_H
#define BB_PERSIST_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/log/log.h"

void logger_init_persist(Logger *logger, LogLevel level);
void logger_free_persist_context(Logger *logger);


#ifdef __cplusplus
}
#endif

#endif //BB_PERSIST_LOGGER_H
