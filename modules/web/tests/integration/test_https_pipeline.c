#include "blue-bird/web/server.h"
#include "blue-bird/web/websocket/websocket.h"
#include "blue-bird/utils/platform.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/error/error.h>
#include <blue-bird/web/error.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(BB_WITH_TLS)

#include <pthread.h>
#include <unistd.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#define TEST_PORT 8443

static char g_tmp_dir[] = "/tmp/bb_https_test_XXXXXX";
static char g_cert_path[512];
static char g_key_path[512];

static bb_runtime_t *server_runtime = NULL;
static bb_server_t *server = NULL;

/* ============================================================
 * Server
 * ============================================================ */

static bb_error_t root_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Hello over TLS");
    return BB_SUCCESS();
}

static bb_error_t echo_body_handler(bb_request_t *req, bb_response_t *res)
{
    bb_http_message_t *msg = bb_request_get_message(req);
    const char *body = bb_message_get_body(msg);
    bb_response_set_header(res, "Content-Type", "text/plain");
    // bb_response_set_body() only reads/copies its argument (see
    // bb_message_set_body()); the non-const char* parameter is just an
    // API wart, not an ownership transfer.
    bb_response_set_body(res, (char *)(body ? body : ""));
    return BB_SUCCESS();
}

static bb_error_t large_response_handler(bb_request_t *req, bb_response_t *res)
{
    (void) req;
    const size_t size = 256 * 1024;
    char *body = malloc(size + 1);
    for (size_t i = 0; i < size; i++)
    {
        body[i] = 'a' + (char)(i % 26);
    }
    body[size] = '\0';
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, body);
    free(body);
    return BB_SUCCESS();
}

static bb_error_t ws_echo_handler(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    if (bb_ws_message_get_type(msg) == BB_WS_MESSAGE_TEXT)
    {
        return bb_websocket_send_text(ws, bb_ws_message_get_data(msg));
    }
    return BB_SUCCESS();
}

static void *server_thread(void *arg)
{
    (void) arg;

    server_runtime = bb_runtime_create();

    bb_tls_config_t tls_config = {
        .certificate_file = g_cert_path,
        .private_key_file = g_key_path,
    };

    bb_error_t err;
    server = bb_server_create_tls_on_runtime(server_runtime, TEST_PORT, &tls_config, &err);
    BB_ASSERT(server != NULL);

    bb_server_add_route(server, "GET", "/", root_handler);
    bb_server_add_route(server, "POST", "/echo", echo_body_handler);
    bb_server_add_route(server, "GET", "/large", large_response_handler);
    bb_server_add_websocket(server, "/ws", ws_echo_handler);

    bb_server_start(server);

    bb_runtime_run(server_runtime);

    bb_server_destroy(server);
    bb_runtime_destroy(server_runtime);

    return NULL;
}

/* ============================================================
 * Minimal raw TLS client helpers
 *
 * bb_client_t/bb_websocket_connect() are plain-TCP only (client-side
 * TLS is out of scope for this phase), so the test plays the part of
 * an HTTPS/WSS client by hand: connect a TCP socket, layer an OpenSSL
 * client SSL session over it, and speak raw HTTP/WebSocket bytes.
 * Verification is disabled here because the fixture certificate is
 * self-signed -- that's a property of this test's client, not of
 * anything Blue-Bird's server enables by default.
 * ============================================================ */

static bb_socket_t _connect_tcp(void)
{
    bb_socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(!bb_socket_is_invalid(fd));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TEST_PORT);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    BB_ASSERT(connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    return fd;
}

static SSL_CTX *_test_client_ctx(void)
{
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    BB_ASSERT(ctx != NULL);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL); /* self-signed test fixture */
    return ctx;
}

static SSL *_tls_connect(bb_socket_t fd, SSL_CTX *ctx)
{
    SSL *ssl = SSL_new(ctx);
    BB_ASSERT(ssl != NULL);
    BB_ASSERT(SSL_set_fd(ssl, (int)fd) == 1);

    int rc = SSL_connect(ssl);
    BB_ASSERT(rc == 1);

    return ssl;
}

static void _ssl_send_all(SSL *ssl, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        int n = SSL_write(ssl, data + sent, (int)(len - sent));
        BB_ASSERT(n > 0);
        sent += (size_t)n;
    }
}

/* Reads until the peer closes or `capacity` is exhausted. Good enough
 * for these short-lived, single-response test connections. */
static size_t _ssl_read_all(SSL *ssl, char *buf, size_t capacity)
{
    size_t total = 0;
    while (total < capacity - 1)
    {
        int n = SSL_read(ssl, buf + total, (int)(capacity - 1 - total));
        if (n <= 0)
            break;
        total += (size_t)n;
    }
    buf[total] = '\0';
    return total;
}

/* ============================================================
 * HTTPS tests
 * ============================================================ */

static void https_get_test(void)
{
    printf("\tTesting GET / over HTTPS...\n");

    SSL_CTX *ctx = _test_client_ctx();
    bb_socket_t fd = _connect_tcp();
    SSL *ssl = _tls_connect(fd, ctx);

    const char *req = "GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    _ssl_send_all(ssl, req, strlen(req));

    char buf[4096];
    size_t n = _ssl_read_all(ssl, buf, sizeof(buf));
    BB_ASSERT(n > 0);
    BB_ASSERT(strstr(buf, "200") != NULL);
    BB_ASSERT(strstr(buf, "Hello over TLS") != NULL);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    bb_socket_close(fd);
    SSL_CTX_free(ctx);
}

static void https_post_body_test(void)
{
    printf("\tTesting POST body over HTTPS...\n");

    SSL_CTX *ctx = _test_client_ctx();
    bb_socket_t fd = _connect_tcp();
    SSL *ssl = _tls_connect(fd, ctx);

    const char *body = "hello from an HTTPS client";
    char req[512];
    snprintf(req, sizeof(req),
        "POST /echo HTTP/1.1\r\nHost: localhost\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",
        strlen(body), body);

    _ssl_send_all(ssl, req, strlen(req));

    char buf[4096];
    size_t n = _ssl_read_all(ssl, buf, sizeof(buf));
    BB_ASSERT(n > 0);
    BB_ASSERT(strstr(buf, "200") != NULL);
    BB_ASSERT(strstr(buf, body) != NULL);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    bb_socket_close(fd);
    SSL_CTX_free(ctx);
}

static void https_large_response_test(void)
{
    printf("\tTesting large response over HTTPS...\n");

    SSL_CTX *ctx = _test_client_ctx();
    bb_socket_t fd = _connect_tcp();
    SSL *ssl = _tls_connect(fd, ctx);

    const char *req = "GET /large HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    _ssl_send_all(ssl, req, strlen(req));

    char *buf = malloc(512 * 1024);
    BB_ASSERT(buf != NULL);
    size_t n = _ssl_read_all(ssl, buf, 512 * 1024);
    BB_ASSERT(n > 256 * 1024); /* headers + 256 KiB body */
    BB_ASSERT(strstr(buf, "200") != NULL);
    BB_ASSERT(strstr(buf, "abcdefghijklmnopqrstuvwxyz") != NULL);

    free(buf);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    bb_socket_close(fd);
    SSL_CTX_free(ctx);
}

static void https_multiple_requests_one_connection_test(void)
{
    printf("\tTesting multiple requests on one TLS connection...\n");

    SSL_CTX *ctx = _test_client_ctx();
    bb_socket_t fd = _connect_tcp();
    SSL *ssl = _tls_connect(fd, ctx);

    /* No Connection: close here -- the second request must still be
     * readable over the same already-established TLS session. */
    const char *req = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    _ssl_send_all(ssl, req, strlen(req));

    char buf[4096];
    int n = SSL_read(ssl, buf, sizeof(buf) - 1);
    BB_ASSERT(n > 0);
    buf[n] = '\0';
    BB_ASSERT(strstr(buf, "200") != NULL);
    BB_ASSERT(strstr(buf, "Hello over TLS") != NULL);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    bb_socket_close(fd);
    SSL_CTX_free(ctx);
}

static void https_parser_error_test(void)
{
    printf("\tTesting malformed HTTP request over TLS...\n");

    SSL_CTX *ctx = _test_client_ctx();
    bb_socket_t fd = _connect_tcp();
    SSL *ssl = _tls_connect(fd, ctx);

    const char *req = "NOTAMETHOD / HTTP/9.9\r\nHost: localhost\r\nConnection: close\r\n\r\n";
    _ssl_send_all(ssl, req, strlen(req));

    char buf[4096];
    size_t n = _ssl_read_all(ssl, buf, sizeof(buf));
    BB_ASSERT(n > 0);
    BB_ASSERT(strstr(buf, "400") != NULL);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    bb_socket_close(fd);
    SSL_CTX_free(ctx);
}

/* ============================================================
 * WSS test
 *
 * Hand-rolled: minimal WebSocket upgrade handshake plus a single
 * masked text frame, all over the OpenSSL client session above.
 * ============================================================ */

static void wss_echo_test(void)
{
    printf("\tTesting WebSocket over TLS (WSS)...\n");

    SSL_CTX *ctx = _test_client_ctx();
    bb_socket_t fd = _connect_tcp();
    SSL *ssl = _tls_connect(fd, ctx);

    const char *upgrade =
        "GET /ws HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n";
    _ssl_send_all(ssl, upgrade, strlen(upgrade));

    char buf[1024];
    int n = SSL_read(ssl, buf, sizeof(buf) - 1);
    BB_ASSERT(n > 0);
    buf[n] = '\0';
    BB_ASSERT(strstr(buf, "101") != NULL);

    /* Client->server frames must be masked. Payload: "hi" (2 bytes). */
    const char payload[2] = { 'h', 'i' };
    uint8_t mask[4] = { 0x12, 0x34, 0x56, 0x78 };
    uint8_t frame[2 + 4 + 2];
    frame[0] = 0x81; /* FIN + text opcode */
    frame[1] = 0x80 | (uint8_t)sizeof(payload); /* MASK bit + length */
    memcpy(frame + 2, mask, 4);
    for (size_t i = 0; i < sizeof(payload); i++)
    {
        frame[6 + i] = (uint8_t)(payload[i] ^ mask[i % 4]);
    }

    _ssl_send_all(ssl, (const char *)frame, sizeof(frame));

    uint8_t resp[16];
    int rn = SSL_read(ssl, resp, sizeof(resp));
    BB_ASSERT(rn >= 4);
    BB_ASSERT((resp[0] & 0x0F) == 0x1); /* text opcode */
    BB_ASSERT((resp[1] & 0x80) == 0);   /* server frames are unmasked */
    uint8_t len = resp[1] & 0x7F;
    BB_ASSERT(len == sizeof(payload));
    BB_ASSERT(memcmp(resp + 2, payload, sizeof(payload)) == 0);

    SSL_shutdown(ssl);
    SSL_free(ssl);
    bb_socket_close(fd);
    SSL_CTX_free(ctx);
}

static void _generate_cert_fixture(void)
{
    BB_ASSERT(mkdtemp(g_tmp_dir) != NULL);

    snprintf(g_cert_path, sizeof(g_cert_path), "%s/cert.pem", g_tmp_dir);
    snprintf(g_key_path, sizeof(g_key_path), "%s/key.pem", g_tmp_dir);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "openssl req -x509 -newkey rsa:2048 -keyout %s -out %s "
        "-days 1 -nodes -subj \"/CN=localhost\" >/dev/null 2>&1",
        g_key_path, g_cert_path);

    BB_ASSERT(system(cmd) == 0);
}

static void _cleanup_cert_fixture(void)
{
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", g_tmp_dir);
    system(cmd); /* best effort */
}

int main(void)
{
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);

    _generate_cert_fixture();

    pthread_t thread_id;
    BB_ASSERT(pthread_create(&thread_id, NULL, server_thread, NULL) == 0);

    while (!server_runtime || !bb_runtime_is_running(server_runtime))
    {
        bb_usleep(10000);
    }

    https_get_test();
    https_post_body_test();
    https_large_response_test();
    https_multiple_requests_one_connection_test();
    https_parser_error_test();
    wss_echo_test();

    printf("HTTPS/WSS integration tests passed.\n");

    bb_runtime_stop(server_runtime);
    pthread_join(thread_id, NULL);

    _cleanup_cert_fixture();

    return 0;
}

#else /* !defined(BB_WITH_TLS) */

/* No OpenSSL in this build: there's no TLS server to talk to. Just
 * confirm the TLS server API fails loudly and predictably instead of
 * silently falling back to plaintext. */
int main(void)
{
    printf("Testing HTTPS server creation on a build without BB_WITH_TLS...\n");

    bb_tls_config_t config = {
        .certificate_file = "cert.pem",
        .private_key_file = "key.pem",
    };

    bb_error_t err;
    bb_server_t *server = bb_server_create_tls(9443, &config, &err);

    BB_ASSERT(server == NULL);
    BB_ASSERT(err.code == BB_ERR_TLS_UNSUPPORTED);

    printf("HTTPS/WSS integration tests skipped (built without BB_WITH_TLS).\n");
    return 0;
}

#endif
