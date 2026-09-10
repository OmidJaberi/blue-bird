#include "blue-bird/web/tls.h"
#include "blue-bird/web/server.h"
#include "blue-bird/web/error.h"
#include "blue-bird/utils/bb_config.h"
#include "blue-bird/utils/platform.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/error/error.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * bb_tls_config_from_json()
 * ============================================================ */

static bb_json_t *_make_config(const char *cert, const char *key)
{
    bb_json_t *config = bb_json_create(BB_JSON_OBJECT);
    BB_ASSERT(config != NULL);

    if (cert)
    {
        BB_ASSERT(!BB_FAILED(bb_json_object_set_value(
            config, BB_CONFIG_KEY_TLS_CERTIFICATE_FILE, bb_json_new_text(cert))));
    }

    if (key)
    {
        BB_ASSERT(!BB_FAILED(bb_json_object_set_value(
            config, BB_CONFIG_KEY_TLS_PRIVATE_KEY_FILE, bb_json_new_text(key))));
    }

    return config;
}

static void tls_config_from_json_valid_test(void)
{
    printf("\tTesting bb_tls_config_from_json() with a valid config...\n");

    bb_json_t *config = _make_config("cert.pem", "key.pem");

    bb_tls_config_t tls_config;
    bb_error_t err = bb_tls_config_from_json(config, &tls_config);

    BB_ASSERT(!BB_FAILED(err));
    BB_ASSERT(strcmp(tls_config.certificate_file, "cert.pem") == 0);
    BB_ASSERT(strcmp(tls_config.private_key_file, "key.pem") == 0);

    bb_json_destroy(config);
}

static void tls_config_from_json_missing_certificate_test(void)
{
    printf("\tTesting bb_tls_config_from_json() with a missing certificate key...\n");

    bb_json_t *config = _make_config(NULL, "key.pem");

    bb_tls_config_t tls_config;
    bb_error_t err = bb_tls_config_from_json(config, &tls_config);

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);

    bb_json_destroy(config);
}

static void tls_config_from_json_missing_private_key_test(void)
{
    printf("\tTesting bb_tls_config_from_json() with a missing private-key key...\n");

    bb_json_t *config = _make_config("cert.pem", NULL);

    bb_tls_config_t tls_config;
    bb_error_t err = bb_tls_config_from_json(config, &tls_config);

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);

    bb_json_destroy(config);
}

static void tls_config_from_json_empty_string_test(void)
{
    printf("\tTesting bb_tls_config_from_json() with an empty-string value...\n");

    bb_json_t *config = _make_config("", "key.pem");

    bb_tls_config_t tls_config;
    bb_error_t err = bb_tls_config_from_json(config, &tls_config);

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);

    bb_json_destroy(config);
}

static void tls_config_from_json_wrong_type_test(void)
{
    printf("\tTesting bb_tls_config_from_json() with a non-string value...\n");

    bb_json_t *config = bb_json_create(BB_JSON_OBJECT);
    BB_ASSERT(!BB_FAILED(bb_json_object_set_value(
        config, BB_CONFIG_KEY_TLS_CERTIFICATE_FILE, bb_json_new_int(42))));
    BB_ASSERT(!BB_FAILED(bb_json_object_set_value(
        config, BB_CONFIG_KEY_TLS_PRIVATE_KEY_FILE, bb_json_new_text("key.pem"))));

    bb_tls_config_t tls_config;
    bb_error_t err = bb_tls_config_from_json(config, &tls_config);

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);

    bb_json_destroy(config);
}

static void tls_config_from_json_null_config_test(void)
{
    printf("\tTesting bb_tls_config_from_json() with a NULL config...\n");

    bb_tls_config_t tls_config;
    bb_error_t err = bb_tls_config_from_json(NULL, &tls_config);

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);
}

static void tls_config_from_json_null_output_test(void)
{
    printf("\tTesting bb_tls_config_from_json() with a NULL output...\n");

    bb_json_t *config = _make_config("cert.pem", "key.pem");

    bb_error_t err = bb_tls_config_from_json(config, NULL);

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);

    bb_json_destroy(config);
}

static void tls_config_from_json_via_env_file_test(void)
{
    printf("\tTesting bb_tls_config_from_json() with a config loaded from a .env file...\n");

    const char *path = "test_tls_config.env";

    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);
    fputs(
        BB_CONFIG_KEY_TLS_CERTIFICATE_FILE "=cert.pem\n"
        BB_CONFIG_KEY_TLS_PRIVATE_KEY_FILE "=key.pem\n",
        f);
    fclose(f);

    bb_json_t *config = bb_json_create(BB_JSON_OBJECT);
    BB_ASSERT(!BB_FAILED(bb_config_load_env(config, path)));

    bb_tls_config_t tls_config;
    bb_error_t err = bb_tls_config_from_json(config, &tls_config);

    BB_ASSERT(!BB_FAILED(err));
    BB_ASSERT(strcmp(tls_config.certificate_file, "cert.pem") == 0);
    BB_ASSERT(strcmp(tls_config.private_key_file, "key.pem") == 0);

    remove(path);
    bb_json_destroy(config);
}

/* ============================================================
 * Creating a TLS server from a bb_config-derived TLS config
 * ============================================================ */

#if defined(BB_WITH_TLS)

#if defined(_WIN32)
#define _BB_NULL_REDIRECT ">NUL 2>&1"
#else
#define _BB_NULL_REDIRECT ">/dev/null 2>&1"
#endif

#define TEST_PORT 18443

/* Fixed, non-unique directory name: this test binary runs once per
 * ctest invocation, so a temp-unique name isn't needed -- just don't
 * collide with other test fixtures. */
static char g_tmp_dir[] = "bb_tls_config_test_fixtures";
static char g_cert_path[512];
static char g_key_path[512];

static void _run(const char *cmd)
{
    int rc = system(cmd);
    BB_ASSERT(rc == 0);
}

static void _generate_fixtures(void)
{
    remove(g_tmp_dir);
    bb_mkdir(g_tmp_dir);

    snprintf(g_cert_path, sizeof(g_cert_path), "%s/cert.pem", g_tmp_dir);
    snprintf(g_key_path, sizeof(g_key_path), "%s/key.pem", g_tmp_dir);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -keyout %s -out %s "
        "-days 1 -nodes -subj \"/CN=localhost\" " _BB_NULL_REDIRECT,
        g_key_path, g_cert_path);
    _run(cmd);
}

static void _cleanup_fixtures(void)
{
    remove(g_cert_path);
    remove(g_key_path);
#if defined(_WIN32)
    _rmdir(g_tmp_dir);
#else
    rmdir(g_tmp_dir);
#endif
}

static void tls_server_created_from_config_test(void)
{
    printf("\tTesting TLS server creation from a bb_config-derived TLS config...\n");

    bb_json_t *config = _make_config(g_cert_path, g_key_path);

    bb_tls_config_t tls_config;
    BB_ASSERT(!BB_FAILED(bb_tls_config_from_json(config, &tls_config)));

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    bb_error_t err;
    bb_server_t *server = bb_server_create_tls_on_runtime(runtime, TEST_PORT, &tls_config, &err);

    BB_ASSERT(server != NULL);
    BB_ASSERT(err.code == BB_OK);

    bb_server_destroy(server);
    bb_runtime_destroy(runtime);
    bb_json_destroy(config);
}

static void tls_server_from_invalid_config_test(void)
{
    printf("\tTesting TLS server creation from an incomplete bb_config...\n");

    // Only the certificate path is present -- bb_tls_config_from_json()
    // must fail before a server is ever attempted.
    bb_json_t *config = _make_config(g_cert_path, NULL);

    bb_tls_config_t tls_config;
    bb_error_t err = bb_tls_config_from_json(config, &tls_config);

    BB_ASSERT(BB_FAILED(err));
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);

    bb_json_destroy(config);
}

static void tls_server_from_config_with_missing_files_test(void)
{
    printf("\tTesting TLS server creation from config pointing at missing files...\n");

    bb_json_t *config = _make_config("does_not_exist_cert.pem", "does_not_exist_key.pem");

    bb_tls_config_t tls_config;
    BB_ASSERT(!BB_FAILED(bb_tls_config_from_json(config, &tls_config)));

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    bb_error_t err;
    bb_server_t *server = bb_server_create_tls_on_runtime(runtime, TEST_PORT, &tls_config, &err);

    // bb_tls_config_from_json() only checks that the config carries
    // non-empty strings -- actual file validation happens here, same
    // as it always has for a hand-built bb_tls_config_t.
    BB_ASSERT(server == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);

    bb_runtime_destroy(runtime);
    bb_json_destroy(config);
}

#else /* !defined(BB_WITH_TLS) */

static void tls_server_from_config_unsupported_build_test(void)
{
    printf("\tTesting TLS server creation from config on a build without BB_WITH_TLS...\n");

    bb_json_t *config = _make_config("cert.pem", "key.pem");

    bb_tls_config_t tls_config;
    BB_ASSERT(!BB_FAILED(bb_tls_config_from_json(config, &tls_config)));

    bb_runtime_t *runtime = bb_runtime_create();
    BB_ASSERT(runtime != NULL);

    bb_error_t err;
    bb_server_t *server = bb_server_create_tls_on_runtime(runtime, 18443, &tls_config, &err);

    BB_ASSERT(server == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_UNSUPPORTED);

    bb_runtime_destroy(runtime);
    bb_json_destroy(config);
}

#endif

int main(void)
{
    printf("Running TLS config tests...\n");

    tls_config_from_json_valid_test();
    tls_config_from_json_missing_certificate_test();
    tls_config_from_json_missing_private_key_test();
    tls_config_from_json_empty_string_test();
    tls_config_from_json_wrong_type_test();
    tls_config_from_json_null_config_test();
    tls_config_from_json_null_output_test();
    tls_config_from_json_via_env_file_test();

#if defined(BB_WITH_TLS)
    _generate_fixtures();

    tls_server_created_from_config_test();
    tls_server_from_invalid_config_test();
    tls_server_from_config_with_missing_files_test();

    _cleanup_fixtures();
#else
    tls_server_from_config_unsupported_build_test();
#endif

    printf("All tests passed.\n");
    return 0;
}
