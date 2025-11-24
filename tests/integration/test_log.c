#include "log/log.h"
#include "log/console_logger.h"
#include "log/persist_logger.h"
#include "persist/persist.h"
#include "persist/persist_file.h"
#include <stdio.h>

int main(void)
{
    Logger console_logger;
    Logger persist_logger;

    // Initialize console logger
    logger_init_console(&console_logger, LOG_LEVEL_INFO, stderr);

    // Register persist backend and set default
    persist_file_register();
    persist_set_default("file");
    persist_set_default_uri("logs");  // directory to store logs

    // Initialize persist logger
    logger_init_persist(&persist_logger, LOG_LEVEL_DEBUG);

    // Default logger for macros
    default_logger = console_logger;

    LOG_INFO("Server started");
    logger_log(&persist_logger, LOG_LEVEL_DEBUG, "Debug info saved to persist log");
    logger_log(&persist_logger, LOG_LEVEL_DEBUG, "Another log appended to persist log");
    logger_log(&persist_logger, LOG_LEVEL_DEBUG, "And a third log ...");

    // Cleanup
    logger_close(&console_logger);
    logger_free_persist_context(&persist_logger);

    return 0;
}
