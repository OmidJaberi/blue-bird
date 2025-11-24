#ifndef BLUE_BIRD_CONSOLE_LOGGER_H
#define BLUE_BIRD_CONSOLE_LOGGER_H

#include "log.h"
#include <stdio.h>

void logger_init_console(Logger *logger, LogLevel level, FILE *out);

#endif /* BLUE_BIRD_CONSOLE_LOGGER_H */
