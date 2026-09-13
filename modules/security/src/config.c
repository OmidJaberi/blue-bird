#include "blue-bird/security/config.h"

#include "../internal/security_internal.h"

static bb_security_config_t g_active_config;
static int g_active_config_initialized = 0;

void bb_security_config_default(bb_security_config_t *out_config)
{
    out_config->password_pbkdf2_iterations = BB_PASSWORD_PBKDF2_ITERATIONS_DEFAULT;
    out_config->password_max_length = BB_PASSWORD_MAX_LENGTH_DEFAULT;
    out_config->session_lifetime_seconds = BB_SESSION_LIFETIME_DEFAULT_SECONDS;
    out_config->session_id_length = BB_SESSION_ID_LENGTH_DEFAULT;
}

bb_error_t bb_security_config_validate(const bb_security_config_t *config)
{
    if (!config)
        return BB_ERROR(BB_ERR_NULL, "null config");

    if (config->password_pbkdf2_iterations < BB_PASSWORD_PBKDF2_ITERATIONS_MIN)
        return BB_ERROR(BB_ERR_INVALID_SECURITY_CONFIG, "password_pbkdf2_iterations is below the minimum");

    if (config->password_max_length < BB_PASSWORD_MAX_LENGTH_MIN ||
        config->password_max_length > BB_PASSWORD_MAX_LENGTH_CEILING)
        return BB_ERROR(BB_ERR_INVALID_SECURITY_CONFIG, "password_max_length is out of range");

    if (config->session_lifetime_seconds < BB_SESSION_LIFETIME_MIN_SECONDS ||
        config->session_lifetime_seconds > BB_SESSION_LIFETIME_MAX_SECONDS)
        return BB_ERROR(BB_ERR_INVALID_SECURITY_CONFIG, "session_lifetime_seconds is out of range");

    if (config->session_id_length < BB_SESSION_ID_LENGTH_MIN ||
        config->session_id_length > BB_SESSION_ID_LENGTH_MAX)
        return BB_ERROR(BB_ERR_INVALID_SECURITY_CONFIG, "session_id_length is out of range");

    return BB_SUCCESS();
}

bb_error_t bb_security_config_set(const bb_security_config_t *config)
{
    bb_error_t err = bb_security_config_validate(config);

    if (BB_FAILED(err))
        return err;

    g_active_config = *config;
    g_active_config_initialized = 1;

    return BB_SUCCESS();
}

const bb_security_config_t *bb_security_config_get(void)
{
    if (!g_active_config_initialized)
    {
        bb_security_config_default(&g_active_config);
        g_active_config_initialized = 1;
    }

    return &g_active_config;
}

/*
 * Reads `key` out of `config` as a long, accepting either a JSON
 * integer or real (a config file might reasonably write either). If
 * `key` is present but holds some other JSON type (e.g. a quoted
 * string), that's almost certainly a config mistake - returning -1
 * makes it manifest as an out-of-range value for every field we use it
 * for, so bb_security_config_validate() catches it below rather than
 * silently keeping the default.
 */
static int get_optional_long(bb_json_t *config, const char *key, long *out)
{
    bb_json_t *node = bb_json_object_get_value(config, key);

    if (!node)
        return 0;

    bb_json_type_t type = bb_json_get_type(node);

    if (type == BB_JSON_INT)
    {
        *out = (long)bb_json_get_value_integer(node);
    }
    else if (type == BB_JSON_REAL)
    {
        *out = (long)bb_json_get_value_real(node);
    }
    else
    {
        *out = -1;
    }

    return 1;
}

bb_error_t bb_security_config_from_json(bb_json_t *config, bb_security_config_t *out_config)
{
    if (!config || !out_config)
        return BB_ERROR(BB_ERR_NULL, "null config or output");

    bb_security_config_default(out_config);

    long value;

    if (get_optional_long(config, BB_CONFIG_KEY_SECURITY_PASSWORD_ITERATIONS, &value))
        out_config->password_pbkdf2_iterations = (value > 0) ? (uint32_t)value : 0;

    if (get_optional_long(config, BB_CONFIG_KEY_SECURITY_PASSWORD_MAX_LENGTH, &value))
        out_config->password_max_length = (value > 0) ? (size_t)value : 0;

    if (get_optional_long(config, BB_CONFIG_KEY_SECURITY_SESSION_LIFETIME_SECONDS, &value))
        out_config->session_lifetime_seconds = (time_t)value;

    if (get_optional_long(config, BB_CONFIG_KEY_SECURITY_SESSION_ID_LENGTH, &value))
        out_config->session_id_length = (value > 0) ? (size_t)value : 0;

    bb_error_t err = bb_security_config_validate(out_config);

    if (BB_FAILED(err))
    {
        bb_security_config_default(out_config);
        return err;
    }

    return BB_SUCCESS();
}
