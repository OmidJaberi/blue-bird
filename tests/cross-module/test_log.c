#include "blue-bird/log/log.h"
#include "blue-bird/log/console_logger.h"
#include "blue-bird/log/persist_logger.h"
#include "blue-bird/persist/key_val.h"
#include "blue-bird/persist/key_val/persist_file.h"
#include <stdio.h>

int main(void)
{
    bb_logger_t console_logger;
    bb_logger_t persist_logger;

    // Initialize console logger
    bb_logger_init_console(&console_logger, BB_LOG_LEVEL_INFO, stderr);

    // Register persist backend and set default
    bb_persist_kv_file_register();
    bb_persist_kv_set_default("file");
    bb_persist_kv_set_default_uri("logs");  // directory to store logs

    // Initialize persist logger
    bb_logger_init_persist(&persist_logger, BB_LOG_LEVEL_DEBUG);

    // Default logger for macros
    default_logger = console_logger;

    BB_LOG_INFO("Server started");
    bb_logger_log(&persist_logger, BB_LOG_LEVEL_DEBUG, "Debug info saved to persist log");
    bb_logger_log(&persist_logger, BB_LOG_LEVEL_DEBUG, "Another log appended to persist log");
    bb_logger_log(&persist_logger, BB_LOG_LEVEL_DEBUG, "And a third log ...");

    // Cleanup
    bb_logger_close(&console_logger);
    bb_logger_free_persist_context(&persist_logger);

    return 0;
}
