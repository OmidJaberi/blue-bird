#include "blue-bird/security/config.h"
#include "blue-bird/security/session.h" /* BB_ERR_INVALID_SECURITY_CONFIG */

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/json.h>
#include <stdio.h>

void test_default_is_valid(void)
{
    printf("\tTesting that the default config passes validation...\n");

    bb_security_config_t config;

    bb_security_config_default(&config);

    BB_ASSERT(bb_security_config_validate(&config).code == BB_OK);
}

void test_validate_null(void)
{
    printf("\tTesting validate() with a null config...\n");

    BB_ASSERT(bb_security_config_validate(NULL).code == BB_ERR_NULL);
}

void test_validate_rejects_low_iterations(void)
{
    printf("\tTesting rejection of a too-low iteration count...\n");

    bb_security_config_t config;
    bb_security_config_default(&config);

    config.password_pbkdf2_iterations = 1;

    BB_ASSERT(bb_security_config_validate(&config).code == BB_ERR_INVALID_SECURITY_CONFIG);
}

void test_validate_rejects_bad_password_max_length(void)
{
    printf("\tTesting rejection of an out-of-range password_max_length...\n");

    bb_security_config_t config;

    bb_security_config_default(&config);
    config.password_max_length = 0;
    BB_ASSERT(bb_security_config_validate(&config).code == BB_ERR_INVALID_SECURITY_CONFIG);

    bb_security_config_default(&config);
    config.password_max_length = 1000000;
    BB_ASSERT(bb_security_config_validate(&config).code == BB_ERR_INVALID_SECURITY_CONFIG);
}

void test_validate_rejects_bad_session_lifetime(void)
{
    printf("\tTesting rejection of an out-of-range session_lifetime_seconds...\n");

    bb_security_config_t config;

    bb_security_config_default(&config);
    config.session_lifetime_seconds = 0;
    BB_ASSERT(bb_security_config_validate(&config).code == BB_ERR_INVALID_SECURITY_CONFIG);

    bb_security_config_default(&config);
    config.session_lifetime_seconds = -5;
    BB_ASSERT(bb_security_config_validate(&config).code == BB_ERR_INVALID_SECURITY_CONFIG);

    bb_security_config_default(&config);
    config.session_lifetime_seconds = 60L * 60 * 24 * 365; /* a full year - too long */
    BB_ASSERT(bb_security_config_validate(&config).code == BB_ERR_INVALID_SECURITY_CONFIG);
}

void test_validate_rejects_bad_session_id_length(void)
{
    printf("\tTesting rejection of an out-of-range session_id_length...\n");

    bb_security_config_t config;

    bb_security_config_default(&config);
    config.session_id_length = 4; /* far below the 128-bit floor */
    BB_ASSERT(bb_security_config_validate(&config).code == BB_ERR_INVALID_SECURITY_CONFIG);

    bb_security_config_default(&config);
    config.session_id_length = 9999; /* doesn't fit in the ID buffer */
    BB_ASSERT(bb_security_config_validate(&config).code == BB_ERR_INVALID_SECURITY_CONFIG);
}

void test_set_and_get_round_trip(void)
{
    printf("\tTesting bb_security_config_set()/_get() round-trip...\n");

    bb_security_config_t config;
    bb_security_config_default(&config);

    config.password_pbkdf2_iterations = 150000;
    config.session_id_length = 40;

    bb_error_t err = bb_security_config_set(&config);
    BB_ASSERT(err.code == BB_OK);

    const bb_security_config_t *active = bb_security_config_get();

    BB_ASSERT(active->password_pbkdf2_iterations == 150000);
    BB_ASSERT(active->session_id_length == 40);

    /* Leave global state clean for any other test that runs in this
     * process. */
    bb_security_config_default(&config);
    bb_security_config_set(&config);
}

void test_set_rejects_invalid_and_leaves_active_config_untouched(void)
{
    printf("\tTesting that set() rejects an invalid config without side effects...\n");

    bb_security_config_t good;
    bb_security_config_default(&good);
    good.password_pbkdf2_iterations = 200000;
    BB_ASSERT(bb_security_config_set(&good).code == BB_OK);

    bb_security_config_t bad;
    bb_security_config_default(&bad);
    bad.password_pbkdf2_iterations = 1; /* invalid */

    bb_error_t err = bb_security_config_set(&bad);
    BB_ASSERT(err.code == BB_ERR_INVALID_SECURITY_CONFIG);

    /* The earlier, valid config must still be active. */
    BB_ASSERT(bb_security_config_get()->password_pbkdf2_iterations == 200000);

    bb_security_config_default(&good);
    bb_security_config_set(&good);
}

void test_from_json_null_arguments(void)
{
    printf("\tTesting from_json() with null arguments...\n");

    bb_security_config_t config;
    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);

    BB_ASSERT(bb_security_config_from_json(NULL, &config).code == BB_ERR_NULL);
    BB_ASSERT(bb_security_config_from_json(obj, NULL).code == BB_ERR_NULL);

    bb_json_destroy(obj);
}

void test_from_json_defaults_when_empty(void)
{
    printf("\tTesting from_json() with no relevant keys present...\n");

    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);

    bb_security_config_t config;
    bb_security_config_t defaults;
    bb_security_config_default(&defaults);

    bb_error_t err = bb_security_config_from_json(obj, &config);

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(config.password_pbkdf2_iterations == defaults.password_pbkdf2_iterations);
    BB_ASSERT(config.password_max_length == defaults.password_max_length);
    BB_ASSERT(config.session_lifetime_seconds == defaults.session_lifetime_seconds);
    BB_ASSERT(config.session_id_length == defaults.session_id_length);

    bb_json_destroy(obj);
}

void test_from_json_partial_override(void)
{
    printf("\tTesting from_json() with only one key overridden...\n");

    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);
    bb_json_object_set_value(obj, BB_CONFIG_KEY_SECURITY_SESSION_ID_LENGTH, bb_json_new_int(48));

    bb_security_config_t config;
    bb_security_config_t defaults;
    bb_security_config_default(&defaults);

    bb_error_t err = bb_security_config_from_json(obj, &config);

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(config.session_id_length == 48);
    /* Everything else should still be the default. */
    BB_ASSERT(config.password_pbkdf2_iterations == defaults.password_pbkdf2_iterations);
    BB_ASSERT(config.password_max_length == defaults.password_max_length);
    BB_ASSERT(config.session_lifetime_seconds == defaults.session_lifetime_seconds);

    bb_json_destroy(obj);
}

void test_from_json_full_override(void)
{
    printf("\tTesting from_json() with every key overridden...\n");

    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);
    bb_json_object_set_value(obj, BB_CONFIG_KEY_SECURITY_PASSWORD_ITERATIONS, bb_json_new_int(150000));
    bb_json_object_set_value(obj, BB_CONFIG_KEY_SECURITY_PASSWORD_MAX_LENGTH, bb_json_new_int(128));
    bb_json_object_set_value(obj, BB_CONFIG_KEY_SECURITY_SESSION_LIFETIME_SECONDS, bb_json_new_int(1800));
    bb_json_object_set_value(obj, BB_CONFIG_KEY_SECURITY_SESSION_ID_LENGTH, bb_json_new_int(32));

    bb_security_config_t config;

    bb_error_t err = bb_security_config_from_json(obj, &config);

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(config.password_pbkdf2_iterations == 150000);
    BB_ASSERT(config.password_max_length == 128);
    BB_ASSERT(config.session_lifetime_seconds == 1800);
    BB_ASSERT(config.session_id_length == 32);

    bb_json_destroy(obj);
}

void test_from_json_out_of_range_value_fails_and_resets_to_defaults(void)
{
    printf("\tTesting from_json() with an out-of-range value...\n");

    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);
    bb_json_object_set_value(obj, BB_CONFIG_KEY_SECURITY_PASSWORD_ITERATIONS, bb_json_new_int(1));

    bb_security_config_t config;
    bb_security_config_t defaults;
    bb_security_config_default(&defaults);

    bb_error_t err = bb_security_config_from_json(obj, &config);

    BB_ASSERT(err.code == BB_ERR_INVALID_SECURITY_CONFIG);
    /* Left holding the (valid) defaults, not a half-applied config. */
    BB_ASSERT(config.password_pbkdf2_iterations == defaults.password_pbkdf2_iterations);
    BB_ASSERT(config.session_id_length == defaults.session_id_length);

    bb_json_destroy(obj);
}

void test_from_json_wrong_type_is_rejected(void)
{
    printf("\tTesting from_json() with a wrong-typed value (string instead of number)...\n");

    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);
    bb_json_object_set_value(obj, BB_CONFIG_KEY_SECURITY_SESSION_ID_LENGTH, bb_json_new_text("forty"));

    bb_security_config_t config;

    bb_error_t err = bb_security_config_from_json(obj, &config);

    BB_ASSERT(err.code == BB_ERR_INVALID_SECURITY_CONFIG);

    bb_json_destroy(obj);
}

int main(void)
{
    printf("Running Security Config tests...\n");

    test_default_is_valid();
    test_validate_null();
    test_validate_rejects_low_iterations();
    test_validate_rejects_bad_password_max_length();
    test_validate_rejects_bad_session_lifetime();
    test_validate_rejects_bad_session_id_length();
    test_set_and_get_round_trip();
    test_set_rejects_invalid_and_leaves_active_config_untouched();
    test_from_json_null_arguments();
    test_from_json_defaults_when_empty();
    test_from_json_partial_override();
    test_from_json_full_override();
    test_from_json_out_of_range_value_fails_and_resets_to_defaults();
    test_from_json_wrong_type_is_rejected();

    printf("All tests passed.\n");

    return 0;
}
