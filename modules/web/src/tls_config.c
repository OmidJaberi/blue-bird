/*
 * Bridges the generic bb_config system (modules/utils/bb_config.h) and
 * bb_tls_config_t.
 *
 * Deliberately independent of BB_WITH_TLS: this only reads strings out
 * of a bb_json_t, it never touches OpenSSL, so it builds and behaves
 * identically whether or not TLS support is compiled in. Actual
 * certificate/key validation still happens exactly where it always
 * has -- in bb_tls_context_create_server() (tls.c / tls_stub.c) during
 * bb_server_create_tls*().
 */

#include "blue-bird/web/tls.h"
#include "blue-bird/web/error.h"

static const char *_get_nonempty_string(bb_json_t *config, const char *key)
{
    bb_json_t *node = bb_json_object_get_value(config, key);

    if (!node)
    {
        return NULL;
    }

    char *value = bb_json_get_value_text(node);

    if (!value || value[0] == '\0')
    {
        return NULL;
    }

    return value;
}

bb_error_t bb_tls_config_from_json(bb_json_t *config, bb_tls_config_t *out_tls_config)
{
    if (!config || !out_tls_config)
    {
        return BB_ERROR(BB_ERR_TLS_CONFIG, "NULL config or output TLS config.");
    }

    const char *certificate_file = _get_nonempty_string(config, BB_CONFIG_KEY_TLS_CERTIFICATE_FILE);

    if (!certificate_file)
    {
        return BB_ERROR(BB_ERR_TLS_CONFIG, "Missing or invalid \"" BB_CONFIG_KEY_TLS_CERTIFICATE_FILE "\" in config.");
    }

    const char *private_key_file = _get_nonempty_string(config, BB_CONFIG_KEY_TLS_PRIVATE_KEY_FILE);

    if (!private_key_file)
    {
        return BB_ERROR(BB_ERR_TLS_CONFIG, "Missing or invalid \"" BB_CONFIG_KEY_TLS_PRIVATE_KEY_FILE "\" in config.");
    }

    out_tls_config->certificate_file = certificate_file;
    out_tls_config->private_key_file = private_key_file;

    return BB_SUCCESS();
}
