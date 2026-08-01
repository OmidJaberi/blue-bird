#ifndef BB_UTILS_CONFIG_H
#define BB_UTILS_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif


#include <blue-bird/error/error.h>
#include "json.h"

/*
 * Load a .env file into an existing JSON object.
 *
 * The destination must be a JSON object.
 * Existing keys are overwritten.
 */
bb_error_t bb_config_load_env(bb_json_t *config,
                              const char *path);

/*
 * Load a JSON configuration file into an existing JSON object.
 *
 * The destination must be a JSON object.
 * Existing keys are overwritten.
 */
bb_error_t bb_config_load_json(bb_json_t *config, const char *path);


#ifdef __cplusplus
}
#endif

#endif //BB_UTILS_CONFIG_H
