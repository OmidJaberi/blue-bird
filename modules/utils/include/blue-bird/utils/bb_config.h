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

/*
 * Well-known config keys for the TLS certificate and private-key
 * file paths.
 *
 * These are loaded like any other value, through the
 * bb_config_load_env()/bb_config_load_json() functions above -- there
 * is no separate TLS-specific loading path. For example, a .env file:
 *
 *   tls_certificate_file=cert.pem
 *   tls_private_key_file=key.pem
 *
 * or the equivalent JSON:
 *
 *   { "tls_certificate_file": "cert.pem", "tls_private_key_file": "key.pem" }
 *
 * The web module's bb_tls_config_from_json() (blue-bird/web/tls.h)
 * turns these keys into a bb_tls_config_t once TLS is in the picture.
 */
#define BB_CONFIG_KEY_TLS_CERTIFICATE_FILE "tls_certificate_file"
#define BB_CONFIG_KEY_TLS_PRIVATE_KEY_FILE "tls_private_key_file"


#ifdef __cplusplus
}
#endif

#endif //BB_UTILS_CONFIG_H
