#ifndef BB_PERSIST_LOGGER_H
#define BB_PERSIST_LOGGER_H

#include "log.h"

void logger_init_persist(Logger *logger, LogLevel level);
void logger_free_persist_context(Logger *logger);

#endif //BB_PERSIST_LOGGER_H
