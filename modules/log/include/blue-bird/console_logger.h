#ifndef BB_CONSOLE_LOGGER_H
#define BB_CONSOLE_LOGGER_H

#include "log.h"
#include <stdio.h>

void logger_init_console(Logger *logger, LogLevel level, FILE *out);

#endif //BB_CONSOLE_LOGGER_H
