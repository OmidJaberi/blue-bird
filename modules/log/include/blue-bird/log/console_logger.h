#ifndef BB_CONSOLE_LOGGER_H
#define BB_CONSOLE_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/log/log.h"
#include <stdio.h>

void logger_init_console(Logger *logger, LogLevel level, FILE *out);


#ifdef __cplusplus
}
#endif

#endif //BB_CONSOLE_LOGGER_H
