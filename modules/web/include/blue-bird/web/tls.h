#ifndef BB_WEB_TLS_H
#define BB_WEB_TLS_H

#ifdef __cplusplus
extern "C" {
#endif


#include <blue-bird/error/error.h>
#include <blue-bird/utils/json.h>
#include <blue-bird/utils/bb_config.h>

/*
 * Server-side TLS configuration.
 *
 * `certificate_file` may be a single certificate or a full chain (PEM,
 * leaf first). `private_key_file` must be the PEM-encoded private key
 * matching that certificate. Both files are loaded and validated during
 * bb_server_create_tls_on_runtime()/bb_server_create_tls() -- startup
 * fails immediately if either file is missing, malformed, or the two
 * don't match, rather than deferring the error to the first client
 * connection.
 */
typedef struct {
    const char *certificate_file;
    const char *private_key_file;
} bb_tls_config_t;

/*
 * Populate `out_tls_config` from a config object, as loaded by
 * bb_config_load_env()/bb_config_load_json() (blue-bird/utils/bb_config.h).
 *
 * Reads the BB_CONFIG_KEY_TLS_CERTIFICATE_FILE and
 * BB_CONFIG_KEY_TLS_PRIVATE_KEY_FILE keys out of `config`. The
 * resulting `certificate_file`/`private_key_file` pointers alias
 * strings owned by `config` -- `config` must outlive `out_tls_config`
 * and any server created from it.
 *
 * This only checks that both keys are present and hold non-empty
 * strings; it does not touch the filesystem. Whether the paths point
 * at an existing, valid, matching certificate/key pair is verified
 * later, during bb_server_create_tls()/bb_server_create_tls_on_runtime()
 * -- exactly as when a bb_tls_config_t is built by hand.
 *
 * Fails with BB_ERR_TLS_CONFIG if `config` or `out_tls_config` is
 * NULL, or if either key is missing or not a non-empty string.
 */
bb_error_t bb_tls_config_from_json(bb_json_t *config, bb_tls_config_t *out_tls_config);


#ifdef __cplusplus
}
#endif

#endif //BB_WEB_TLS_H
