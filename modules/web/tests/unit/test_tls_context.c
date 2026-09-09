#include "transport/transport.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/error/error.h>
#include <blue-bird/web/error.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(BB_WITH_TLS)

#if defined(_WIN32)
#define _BB_NULL_REDIRECT ">NUL 2>&1"
#else
#define _BB_NULL_REDIRECT ">/dev/null 2>&1"
#endif

/* Fixed, non-unique directory name: each of these test binaries runs
 * once per ctest invocation (never in parallel with itself), so a
 * temp-unique name (POSIX mkdtemp(), unavailable on Windows anyway)
 * isn't needed -- just don't collide with other test fixtures. */
static char g_tmp_dir[] = "bb_tls_context_test_fixtures";

static char g_valid_cert[512];
static char g_valid_key[512];
static char g_other_key[512]; /* valid PEM key, but doesn't match g_valid_cert */
static char g_missing_path[512];
static char g_garbage_file[512];

static void _run(const char *cmd)
{
    int rc = system(cmd);
    BB_ASSERT(rc == 0);
}

static void _write_garbage(const char *path)
{
    FILE *f = fopen(path, "w");
    BB_ASSERT(f != NULL);
    fputs("this is not a certificate or key\n", f);
    fclose(f);
}

static void _generate_fixtures(void)
{
    remove(g_tmp_dir); /* in case a stray file exists from a previous run */
    bb_mkdir(g_tmp_dir);

    snprintf(g_valid_cert, sizeof(g_valid_cert), "%s/valid_cert.pem", g_tmp_dir);
    snprintf(g_valid_key, sizeof(g_valid_key), "%s/valid_key.pem", g_tmp_dir);
    snprintf(g_other_key, sizeof(g_other_key), "%s/other_key.pem", g_tmp_dir);
    snprintf(g_missing_path, sizeof(g_missing_path), "%s/does_not_exist.pem", g_tmp_dir);
    snprintf(g_garbage_file, sizeof(g_garbage_file), "%s/garbage.pem", g_tmp_dir);

    char cmd[2048];

    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -keyout %s -out %s "
        "-days 1 -nodes -subj \"/CN=localhost\" " _BB_NULL_REDIRECT,
        g_valid_key, g_valid_cert);
    _run(cmd);

    /* A second, unrelated key -- structurally valid PEM, but does not
     * match g_valid_cert's public key. */
    snprintf(cmd, sizeof(cmd),
        "openssl genrsa -out %s 2048 " _BB_NULL_REDIRECT,
        g_other_key);
    _run(cmd);

    _write_garbage(g_garbage_file);
}

static void _cleanup_fixtures(void)
{
    /* Best effort: remove the files we know we created, then the now-empty
     * directory. Avoids shelling out to rm/rmdir, which differ in syntax
     * between POSIX and Windows shells. */
    remove(g_valid_cert);
    remove(g_valid_key);
    remove(g_other_key);
    remove(g_garbage_file);
#if defined(_WIN32)
    _rmdir(g_tmp_dir);
#else
    rmdir(g_tmp_dir);
#endif
}

static void tls_context_valid_test(void)
{
    printf("\tTesting TLS context creation with valid cert/key...\n");

    bb_tls_config_t config = {
        .certificate_file = g_valid_cert,
        .private_key_file = g_valid_key,
    };

    bb_error_t err;
    bb_tls_context_t *ctx = bb_tls_context_create_server(&config, &err);

    BB_ASSERT(ctx != NULL);
    BB_ASSERT(err.code == BB_OK);

    bb_tls_context_destroy(ctx);
}

static void tls_context_null_out_err_test(void)
{
    printf("\tTesting TLS context creation with NULL out_err...\n");

    bb_tls_config_t config = {
        .certificate_file = g_valid_cert,
        .private_key_file = g_valid_key,
    };

    /* Must not crash when the caller doesn't care about the reason. */
    bb_tls_context_t *ctx = bb_tls_context_create_server(&config, NULL);
    BB_ASSERT(ctx != NULL);
    bb_tls_context_destroy(ctx);
}

static void tls_context_missing_config_test(void)
{
    printf("\tTesting TLS context creation with NULL config...\n");

    bb_error_t err;
    bb_tls_context_t *ctx = bb_tls_context_create_server(NULL, &err);

    BB_ASSERT(ctx == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);
}

static void tls_context_missing_certificate_test(void)
{
    printf("\tTesting TLS context creation with missing certificate file...\n");

    bb_tls_config_t config = {
        .certificate_file = g_missing_path,
        .private_key_file = g_valid_key,
    };

    bb_error_t err;
    bb_tls_context_t *ctx = bb_tls_context_create_server(&config, &err);

    BB_ASSERT(ctx == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);
}

static void tls_context_missing_private_key_test(void)
{
    printf("\tTesting TLS context creation with missing private key file...\n");

    bb_tls_config_t config = {
        .certificate_file = g_valid_cert,
        .private_key_file = g_missing_path,
    };

    bb_error_t err;
    bb_tls_context_t *ctx = bb_tls_context_create_server(&config, &err);

    BB_ASSERT(ctx == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);
}

static void tls_context_invalid_certificate_test(void)
{
    printf("\tTesting TLS context creation with a malformed certificate...\n");

    bb_tls_config_t config = {
        .certificate_file = g_garbage_file,
        .private_key_file = g_valid_key,
    };

    bb_error_t err;
    bb_tls_context_t *ctx = bb_tls_context_create_server(&config, &err);

    BB_ASSERT(ctx == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);
}

static void tls_context_invalid_private_key_test(void)
{
    printf("\tTesting TLS context creation with a malformed private key...\n");

    bb_tls_config_t config = {
        .certificate_file = g_valid_cert,
        .private_key_file = g_garbage_file,
    };

    bb_error_t err;
    bb_tls_context_t *ctx = bb_tls_context_create_server(&config, &err);

    BB_ASSERT(ctx == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);
}

static void tls_context_mismatched_certificate_key_test(void)
{
    printf("\tTesting TLS context creation with mismatched cert/key...\n");

    bb_tls_config_t config = {
        .certificate_file = g_valid_cert,
        .private_key_file = g_other_key,
    };

    bb_error_t err;
    bb_tls_context_t *ctx = bb_tls_context_create_server(&config, &err);

    BB_ASSERT(ctx == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_CONFIG);
}

static void tls_transport_create_test(void)
{
    printf("\tTesting TLS transport creation from a valid context...\n");

    bb_tls_config_t config = {
        .certificate_file = g_valid_cert,
        .private_key_file = g_valid_key,
    };

    bb_error_t err;
    bb_tls_context_t *ctx = bb_tls_context_create_server(&config, &err);
    BB_ASSERT(ctx != NULL);

    bb_socket_t fds[2];
    BB_ASSERT(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    BB_ASSERT(bb_socket_set_nonblocking(fds[0]) == 0);

    bb_transport_t *transport = bb_transport_create_tls_server(fds[0], ctx);
    BB_ASSERT(transport != NULL);

    /* Handshake will not complete against a plain socketpair peer (no
     * TLS client on the other end), but it must not crash and must
     * report a well-formed transport status rather than blocking. */
    bb_transport_status_t st = bb_transport_handshake(transport);
    BB_ASSERT(st == BB_TRANSPORT_WANT_READ || st == BB_TRANSPORT_WANT_WRITE
              || st == BB_TRANSPORT_ERROR || st == BB_TRANSPORT_CLOSED);

    bb_transport_destroy(transport);
    bb_socket_close(fds[0]);
    bb_socket_close(fds[1]);
    bb_tls_context_destroy(ctx);
}

#else /* !defined(BB_WITH_TLS) */

static void tls_context_unsupported_build_test(void)
{
    printf("\tTesting TLS context creation on a build without BB_WITH_TLS...\n");

    bb_tls_config_t config = {
        .certificate_file = "cert.pem",
        .private_key_file = "key.pem",
    };

    bb_error_t err;
    bb_tls_context_t *ctx = bb_tls_context_create_server(&config, &err);

    BB_ASSERT(ctx == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_UNSUPPORTED);

    /* Must also fail gracefully with a NULL config, and never crash. */
    bb_tls_context_t *ctx2 = bb_tls_context_create_server(NULL, NULL);
    BB_ASSERT(ctx2 == NULL);

    /* destroy() on both a NULL and a (never-created) context must be
     * a safe no-op. */
    bb_tls_context_destroy(NULL);
}

#endif

int main(void)
{
    printf("Testing TLS context...\n");

#if defined(BB_WITH_TLS)
    _generate_fixtures();

    tls_context_valid_test();
    tls_context_null_out_err_test();
    tls_context_missing_config_test();
    tls_context_missing_certificate_test();
    tls_context_missing_private_key_test();
    tls_context_invalid_certificate_test();
    tls_context_invalid_private_key_test();
    tls_context_mismatched_certificate_key_test();
    tls_transport_create_test();

    _cleanup_fixtures();
#else
    tls_context_unsupported_build_test();
#endif

    printf("TLS context tests passed.\n");
    return 0;
}
