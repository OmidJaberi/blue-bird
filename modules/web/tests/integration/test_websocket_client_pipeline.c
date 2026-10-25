/*
 * WebSocket CLIENT tested against a scripted raw-socket server.
 *
 * test_websocket_pipeline.c exercises the client only against this project's
 * own server, so a bug shared by both sides (or a gap in client-side
 * validation) cannot show up there. Here the server is a tiny script running on
 * a plain socket, which lets the test assert exactly what the client puts on
 * the wire and how it reacts to what a real, possibly misbehaving, server sends.
 *
 * Tests that document known gaps in the client are opt-in:
 *   BB_WS_STRICT_RFC=1   run the strict RFC 6455 client-conformance group
 */

#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/websocket/websocket.h"
#include "blue-bird/utils/platform.h"
#include "websocket/websocket_internal.h"

#include <blue-bird/error/assert.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32
#include <sys/select.h>
#endif

#define RX_CAP 400000

/* ============================================================
 * Scripted fake server
 * ============================================================ */

typedef struct fake_server fake_server_t;
typedef void (*fake_script_fn)(fake_server_t *fs, bb_socket_t client, void *arg);

struct fake_server
{
    bb_socket_t listen_fd;
    int port;
    pthread_t thread;
    fake_script_fn script;
    void *script_arg;

    volatile int finished;
    volatile int accepted;

    char request[4096]; /* the client's handshake request, NUL-terminated */
    size_t request_len;

    uint8_t *rx; /* bytes received from the client after the handshake */
    size_t rx_len;
};

/* The fake server the current test talks to (one at a time). */
static fake_server_t *g_fs = NULL;

static void fs_send(bb_socket_t fd, const void *data, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = send(fd, (const char *)data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0)
            return; /* client went away; scripts treat that as the end */
        sent += (size_t)n;
    }
}

/* Reads the HTTP request head; false if the client closed or timed out first. */
static bool fs_read_request(fake_server_t *fs, bb_socket_t client)
{
    fs->request_len = 0;
    while (fs->request_len < sizeof(fs->request) - 1)
    {
        ssize_t n = recv(client, fs->request + fs->request_len, 1, 0);
        if (n <= 0)
            return false;
        fs->request_len++;
        fs->request[fs->request_len] = '\0';
        if (fs->request_len >= 4 && memcmp(fs->request + fs->request_len - 4, "\r\n\r\n", 4) == 0)
            return true;
    }
    return false;
}

static bool ci_contains(const char *haystack, const char *needle)
{
    size_t nlen = strlen(needle);
    for (; *haystack; haystack++)
    {
        if (bb_strncasecmp(haystack, needle, nlen) == 0)
            return true;
    }
    return false;
}

static bool ci_equals(const char *a, const char *b)
{
    return strlen(a) == strlen(b) && bb_strncasecmp(a, b, strlen(a)) == 0;
}

/* Value of request header `name` (case-insensitive) copied into out; false if absent. */
static bool request_header(const char *req, const char *name, char *out, size_t cap)
{
    size_t nlen = strlen(name);
    for (const char *line = strstr(req, "\r\n"); line; line = strstr(line + 2, "\r\n"))
    {
        line += 2;
        if (bb_strncasecmp(line, name, nlen) == 0 && line[nlen] == ':')
        {
            const char *v = line + nlen + 1;
            while (*v == ' ')
                v++;
            const char *end = strstr(v, "\r\n");
            size_t vlen = end ? (size_t)(end - v) : strlen(v);
            if (vlen >= cap)
                vlen = cap - 1;
            memcpy(out, v, vlen);
            out[vlen] = '\0';
            return true;
        }
    }
    return false;
}

/* The exact 101 response a conforming server would send for this client's key. */
static size_t fs_build_101(fake_server_t *fs, char *out, size_t cap, bool include_accept, const char *accept_override)
{
    char key[128] = {0};
    BB_ASSERT(request_header(fs->request, "Sec-WebSocket-Key", key, sizeof(key)));

    char *accept = bb_websocket_accept_key(key);
    BB_ASSERT(accept != NULL);

    int n = snprintf(out, cap,
                     "HTTP/1.1 101 Switching Protocols\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "%s%s%s"
                     "\r\n",
                     include_accept ? "Sec-WebSocket-Accept: " : "",
                     include_accept ? (accept_override ? accept_override : accept) : "",
                     include_accept ? "\r\n" : "");
    free(accept);
    BB_ASSERT(n > 0 && (size_t)n < cap);
    return (size_t)n;
}

static void fs_send_101(fake_server_t *fs, bb_socket_t client)
{
    char resp[512];
    size_t n = fs_build_101(fs, resp, sizeof(resp), true, NULL);
    fs_send(client, resp, n);
}

/* Builds an UNMASKED server->client frame; returns its size. */
static size_t build_server_frame(uint8_t *out, uint8_t first_byte, const void *payload, size_t len)
{
    size_t pos = 0;
    out[pos++] = first_byte;
    if (len < 126)
    {
        out[pos++] = (uint8_t)len;
    }
    else if (len <= 0xFFFF)
    {
        out[pos++] = 126;
        out[pos++] = (uint8_t)(len >> 8);
        out[pos++] = (uint8_t)(len & 0xFF);
    }
    else
    {
        out[pos++] = 127;
        for (int shift = 56; shift >= 0; shift -= 8)
            out[pos++] = (uint8_t)((uint64_t)len >> shift);
    }
    memcpy(out + pos, payload, len);
    return pos + len;
}

/* Receives until `want` bytes are in fs->rx, the peer closes, or ~4s pass. */
static void fs_collect(fake_server_t *fs, bb_socket_t client, size_t want)
{
    int idle_ms = 0;
#ifdef _WIN32
    DWORD tv = 200;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#else
    struct timeval tv = {.tv_sec = 0, .tv_usec = 200000};
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    while (fs->rx_len < want && fs->rx_len < RX_CAP && idle_ms < 4000)
    {
        ssize_t n = recv(client, (char *)fs->rx + fs->rx_len, RX_CAP - fs->rx_len, 0);
        if (n > 0)
        {
            fs->rx_len += (size_t)n;
            idle_ms = 0;
        }
        else if (n == 0)
        {
            break;
        }
        else
        {
            idle_ms += 200;
        }
    }
}

static void *fs_thread(void *arg)
{
    fake_server_t *fs = arg;

    fd_set set;
    FD_ZERO(&set);
    FD_SET(fs->listen_fd, &set);
    struct timeval tv = {.tv_sec = 5, .tv_usec = 0};

    if (select((int)fs->listen_fd + 1, &set, NULL, NULL, &tv) > 0)
    {
        bb_socket_t client = accept(fs->listen_fd, NULL, NULL);
        if (!bb_socket_is_invalid(client))
        {
            fs->accepted = 1;
            if (fs_read_request(fs, client) && fs->script)
                fs->script(fs, client, fs->script_arg);
            bb_socket_close(client);
        }
    }

    fs->finished = 1;
    return NULL;
}

static void fs_start(fake_server_t *fs, fake_script_fn script, void *script_arg)
{
    memset(fs, 0, sizeof(*fs));
    fs->script = script;
    fs->script_arg = script_arg;
    fs->rx = malloc(RX_CAP);
    BB_ASSERT(fs->rx != NULL);

    fs->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    BB_ASSERT(!bb_socket_is_invalid(fs->listen_fd));

    int yes = 1;
    setsockopt(fs->listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = 0; /* ephemeral */

    BB_ASSERT(bind(fs->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    BB_ASSERT(listen(fs->listen_fd, 4) == 0);

    socklen_t alen = sizeof(addr);
    BB_ASSERT(getsockname(fs->listen_fd, (struct sockaddr *)&addr, &alen) == 0);
    fs->port = ntohs(addr.sin_port);

    BB_ASSERT(pthread_create(&fs->thread, NULL, fs_thread, fs) == 0);
}

static void fs_stop(fake_server_t *fs)
{
    pthread_join(fs->thread, NULL);
    bb_socket_close(fs->listen_fd);
    free(fs->rx);
}

/* ============================================================
 * Client driver
 * ============================================================ */

typedef struct
{
    int type;
    size_t len;
    uint8_t *data;
} got_msg_t;

typedef struct
{
    bb_runtime_t *runtime;
    bb_websocket_t *ws;

    volatile int connect_called;
    bb_error_t connect_err;
    bb_websocket_t *connect_ws; /* what the connect callback received */

    got_msg_t msgs[32];
    volatile size_t msg_count;

    volatile int pong_count;
    volatile int close_count;

    void (*on_connected)(bb_websocket_t *ws, void *arg);
    void *on_connected_arg;
} client_t;

/* The client the current test drives; message callbacks have no userdata slot. */
static client_t *g_client = NULL;

static bb_error_t client_on_message(bb_websocket_t *ws, const bb_ws_message_t *msg)
{
    (void)ws;
    client_t *c = g_client;

    BB_ASSERT(c->msg_count < sizeof(c->msgs) / sizeof(c->msgs[0]));
    got_msg_t *m = &c->msgs[c->msg_count];
    m->type = bb_ws_message_get_type(msg);
    m->len = bb_ws_message_get_length(msg);
    m->data = malloc(m->len + 1);
    BB_ASSERT(m->data != NULL);
    memcpy(m->data, bb_ws_message_get_data(msg), m->len);
    m->data[m->len] = '\0';
    c->msg_count++;
    return BB_SUCCESS();
}

static void client_on_connect(bb_websocket_t *ws, bb_error_t err, void *userdata)
{
    client_t *c = userdata;
    c->connect_err = err;
    c->connect_ws = ws;
    c->connect_called = 1;

    if (!BB_FAILED(err) && c->on_connected)
        c->on_connected(ws, c->on_connected_arg);
}

static void client_on_pong(bb_websocket_t *ws, const void *payload, size_t length, void *userdata)
{
    (void)ws;
    (void)payload;
    (void)length;
    ((client_t *)userdata)->pong_count++;
}

static void client_on_close(bb_websocket_t *ws, void *userdata)
{
    (void)ws;
    ((client_t *)userdata)->close_count++;
}

static void client_start(client_t *c, const char *url)
{
    memset(c, 0, sizeof(*c));
    g_client = c;

    c->runtime = bb_runtime_create();
    bb_runtime_set_running(c->runtime);

    c->ws = bb_websocket_create_on_runtime(c->runtime);
    bb_websocket_set_message_callback(c->ws, client_on_message, NULL);
    bb_websocket_set_pong_callback(c->ws, client_on_pong, c);
    bb_websocket_set_close_callback(c->ws, client_on_close, c);

    bb_websocket_connect(c->ws, url, client_on_connect, c);
}

/* Ticks the client's runtime until pred() or `seconds` elapse. */
static bool client_run_until(client_t *c, bool (*pred)(client_t *), int seconds)
{
    time_t deadline = time(NULL) + seconds;

    while (!pred(c))
    {
        if (time(NULL) > deadline)
            return false;
        bb_runtime_tick(c->runtime);
    }
    return true;
}

static void client_finish(client_t *c)
{
    for (size_t i = 0; i < c->msg_count; i++)
        free(c->msgs[i].data);

    bb_websocket_destroy(c->ws);
    bb_runtime_destroy(c->runtime);
    g_client = NULL;
}

static bool pred_connected(client_t *c) { return c->connect_called != 0; }

static void make_url(char *out, size_t cap, int port, const char *path)
{
    snprintf(out, cap, "ws://127.0.0.1:%d%s", port, path);
}

/* ============================================================
 * Test 1: what the client puts on the wire
 * ============================================================ */

typedef struct
{
    uint8_t first;
    bool masked;
    uint64_t len;
    int len_form; /* 7, 16 or 64 */
    uint8_t key[4];
    const uint8_t *masked_payload;
    size_t total;
} rx_frame_t;

static bool parse_client_frame(const uint8_t *buf, size_t avail, rx_frame_t *f)
{
    if (avail < 2)
        return false;

    f->first = buf[0];
    f->masked = (buf[1] & 0x80) != 0;
    uint64_t len = buf[1] & 0x7F;
    size_t pos = 2;
    f->len_form = 7;

    if (len == 126)
    {
        if (avail < 4)
            return false;
        len = ((uint64_t)buf[2] << 8) | buf[3];
        pos = 4;
        f->len_form = 16;
    }
    else if (len == 127)
    {
        if (avail < 10)
            return false;
        len = 0;
        for (int i = 0; i < 8; i++)
            len = (len << 8) | buf[2 + i];
        pos = 10;
        f->len_form = 64;
    }

    if (f->masked)
    {
        if (avail < pos + 4)
            return false;
        memcpy(f->key, buf + pos, 4);
        pos += 4;
    }

    if (avail < pos + len)
        return false;

    f->len = len;
    f->masked_payload = buf + pos;
    f->total = pos + (size_t)len;
    return true;
}

#define OUT_FRAMES 8

static const size_t out_text_lens[] = {5, 125, 126, 65535};

typedef struct
{
    char *text[4];
    uint8_t *bin[2];
} out_payloads_t;

static out_payloads_t g_out;

static void send_all_frames(bb_websocket_t *ws, void *arg)
{
    (void)arg;

    for (int i = 0; i < 4; i++)
        BB_ASSERT(!BB_FAILED(bb_websocket_send_text(ws, g_out.text[i])));

    BB_ASSERT(!BB_FAILED(bb_websocket_send_binary(ws, g_out.bin[0], 65536)));
    BB_ASSERT(!BB_FAILED(bb_websocket_send_binary(ws, g_out.bin[1], 70000)));
    BB_ASSERT(!BB_FAILED(bb_websocket_send_ping(ws, "abc", 3)));
    BB_ASSERT(!BB_FAILED(bb_websocket_send_close(ws, 1000, "bye")));
}

static void script_collect_client_frames(fake_server_t *fs, bb_socket_t client, void *arg)
{
    (void)arg;
    fs_send_101(fs, client);

    size_t total = 0;
    for (int i = 0; i < 4; i++)
        total += 2 + (out_text_lens[i] >= 126 ? (out_text_lens[i] > 0xFFFF ? 8 : 2) : 0) + 4 + out_text_lens[i];
    total += (2 + 2 + 4 + 65536) + (2 + 8 + 4 + 70000) + (2 + 4 + 3) + (2 + 4 + 5);

    fs_collect(fs, client, total);
}

static bool pred_server_done(client_t *c)
{
    (void)c;
    return g_fs->finished != 0;
}

static void test_client_frames_on_the_wire(void)
{
    printf("\tTesting client frames: masking, minimal length encoding, opcodes...\n");

    for (int i = 0; i < 4; i++)
    {
        g_out.text[i] = malloc(out_text_lens[i] + 1);
        BB_ASSERT(g_out.text[i] != NULL);
        for (size_t k = 0; k < out_text_lens[i]; k++)
            g_out.text[i][k] = (char)('a' + (k % 26));
        g_out.text[i][out_text_lens[i]] = '\0';
    }
    g_out.bin[0] = malloc(65536);
    g_out.bin[1] = malloc(70000);
    BB_ASSERT(g_out.bin[0] && g_out.bin[1]);
    for (size_t k = 0; k < 65536; k++)
        g_out.bin[0][k] = (uint8_t)(k * 7);
    for (size_t k = 0; k < 70000; k++)
        g_out.bin[1][k] = (uint8_t)(k * 13 + 1);

    fake_server_t fs;
    fs_start(&fs, script_collect_client_frames, NULL);
    g_fs = &fs;

    char url[128];
    make_url(url, sizeof(url), fs.port, "/chat");

    client_t c;
    client_start(&c, url);
    c.on_connected = send_all_frames;

    BB_ASSERT(client_run_until(&c, pred_connected, 5));
    BB_ASSERT(!BB_FAILED(c.connect_err));
    BB_ASSERT(client_run_until(&c, pred_server_done, 8));

    /* The handshake request itself. */
    BB_ASSERT(strncmp(fs.request, "GET /chat HTTP/1.1\r\n", 20) == 0);
    char v[128];
    BB_ASSERT(request_header(fs.request, "Upgrade", v, sizeof(v)) && ci_equals(v, "websocket"));
    BB_ASSERT(request_header(fs.request, "Connection", v, sizeof(v)) && ci_contains(v, "upgrade"));
    BB_ASSERT(request_header(fs.request, "Sec-WebSocket-Version", v, sizeof(v)) && strcmp(v, "13") == 0);
    BB_ASSERT(request_header(fs.request, "Sec-WebSocket-Key", v, sizeof(v)) && strlen(v) == 24);
    BB_ASSERT(request_header(fs.request, "Host", v, sizeof(v)));

    /* Decode every frame the client sent. */
    struct
    {
        uint8_t first;
        size_t len;
        int form;
        const void *expected;
    } expect[OUT_FRAMES] = {
        {0x81, 5, 7, g_out.text[0]},       {0x81, 125, 7, g_out.text[1]},      {0x81, 126, 16, g_out.text[2]},
        {0x81, 65535, 16, g_out.text[3]},  {0x82, 65536, 64, g_out.bin[0]},    {0x82, 70000, 64, g_out.bin[1]},
        {0x89, 3, 7, "abc"},               {0x88, 5, 7, "\x03\xe8" "bye"},
    };

    size_t pos = 0;
    uint32_t keys_seen = 0;
    uint32_t first_key = 0;

    for (int i = 0; i < OUT_FRAMES; i++)
    {
        rx_frame_t f;
        BB_ASSERT(parse_client_frame(fs.rx + pos, fs.rx_len - pos, &f));

        BB_ASSERT(f.first == expect[i].first);          /* FIN set, right opcode, RSV clear */
        BB_ASSERT(f.masked);                            /* RFC 6455 5.1: client frames MUST be masked */
        BB_ASSERT(f.len == expect[i].len);
        BB_ASSERT(f.len_form == expect[i].form);        /* minimal length encoding */

        for (size_t k = 0; k < f.len; k++)
            BB_ASSERT((uint8_t)(f.masked_payload[k] ^ f.key[k & 3]) == ((const uint8_t *)expect[i].expected)[k]);

        uint32_t key;
        memcpy(&key, f.key, 4);
        if (i == 0)
            first_key = key;
        else if (key != first_key)
            keys_seen++;

        pos += f.total;
    }

    BB_ASSERT(pos == fs.rx_len);   /* nothing extra, nothing missing */
    BB_ASSERT(keys_seen > 0);      /* masking keys must not be a constant */

    client_finish(&c);
    fs_stop(&fs);
    g_fs = NULL;

    for (int i = 0; i < 4; i++)
        free(g_out.text[i]);
    free(g_out.bin[0]);
    free(g_out.bin[1]);
}

/* ============================================================
 * Test 2: how the client handles server frames
 * ============================================================ */

static uint8_t g_big_payload[70000];

static void script_server_frames(fake_server_t *fs, bb_socket_t client, void *arg)
{
    (void)arg;
    uint8_t f[80000];
    size_t n;

    fs_send_101(fs, client);
    bb_usleep(30000);

    /* two frames coalesced in one segment */
    n = build_server_frame(f, 0x81, "one", 3);
    n += build_server_frame(f + n, 0x81, "two", 3);
    fs_send(client, f, n);
    bb_usleep(30000);

    /* a frame delivered one byte at a time */
    n = build_server_frame(f, 0x81, "split", 5);
    for (size_t i = 0; i < n; i++)
    {
        fs_send(client, f + i, 1);
        bb_usleep(2000);
    }
    bb_usleep(30000);

    /* 16-bit and 64-bit length binary frames */
    n = build_server_frame(f, 0x82, g_big_payload, 300);
    fs_send(client, f, n);
    n = build_server_frame(f, 0x82, g_big_payload, 70000);
    fs_send(client, f, n);
    bb_usleep(30000);

    /* Ping -> the client must answer with a masked Pong carrying the same payload */
    n = build_server_frame(f, 0x89, "p1", 2);
    fs_send(client, f, n);
    uint8_t max_ping[125];
    memset(max_ping, 'm', sizeof(max_ping));
    n = build_server_frame(f, 0x89, max_ping, sizeof(max_ping));
    fs_send(client, f, n);

    /* Close(1000): the client should answer with a Close of its own */
    const uint8_t code[2] = {0x03, 0xE8};
    n = build_server_frame(f, 0x88, code, sizeof(code));
    fs_send(client, f, n);

    /* Expected from the client: pong(2) + pong(125) + close(>=2) */
    fs_collect(fs, client, (2 + 4 + 2) + (2 + 4 + 125) + (2 + 4 + 2));
}

static bool pred_seven_messages(client_t *c) { return c->msg_count >= 5; }

static void test_client_handles_server_frames(void)
{
    printf("\tTesting client receives coalesced, split, 16/64-bit frames and answers Ping/Close...\n");

    for (size_t i = 0; i < sizeof(g_big_payload); i++)
        g_big_payload[i] = (uint8_t)(i * 31 + 7);

    fake_server_t fs;
    fs_start(&fs, script_server_frames, NULL);
    g_fs = &fs;

    char url[128];
    make_url(url, sizeof(url), fs.port, "/chat");

    client_t c;
    client_start(&c, url);

    BB_ASSERT(client_run_until(&c, pred_connected, 5));
    BB_ASSERT(!BB_FAILED(c.connect_err));
    BB_ASSERT(client_run_until(&c, pred_seven_messages, 8));
    BB_ASSERT(client_run_until(&c, pred_server_done, 8));

    BB_ASSERT(c.msg_count == 5);
    BB_ASSERT(c.msgs[0].type == BB_WS_MESSAGE_TEXT && c.msgs[0].len == 3 && memcmp(c.msgs[0].data, "one", 3) == 0);
    BB_ASSERT(c.msgs[1].type == BB_WS_MESSAGE_TEXT && c.msgs[1].len == 3 && memcmp(c.msgs[1].data, "two", 3) == 0);
    BB_ASSERT(c.msgs[2].type == BB_WS_MESSAGE_TEXT && c.msgs[2].len == 5 && memcmp(c.msgs[2].data, "split", 5) == 0);
    BB_ASSERT(c.msgs[3].type == BB_WS_MESSAGE_BINARY && c.msgs[3].len == 300 &&
              memcmp(c.msgs[3].data, g_big_payload, 300) == 0);
    BB_ASSERT(c.msgs[4].type == BB_WS_MESSAGE_BINARY && c.msgs[4].len == 70000 &&
              memcmp(c.msgs[4].data, g_big_payload, 70000) == 0);

    /* What the client sent back: Pong("p1"), Pong(125 x 'm'), then a Close. */
    size_t pos = 0;
    rx_frame_t f;

    BB_ASSERT(parse_client_frame(fs.rx + pos, fs.rx_len - pos, &f));
    BB_ASSERT(f.first == 0x8A && f.masked && f.len == 2);
    BB_ASSERT((uint8_t)(f.masked_payload[0] ^ f.key[0]) == 'p' && (uint8_t)(f.masked_payload[1] ^ f.key[1]) == '1');
    pos += f.total;

    BB_ASSERT(parse_client_frame(fs.rx + pos, fs.rx_len - pos, &f));
    BB_ASSERT(f.first == 0x8A && f.masked && f.len == 125);
    for (size_t k = 0; k < f.len; k++)
        BB_ASSERT((uint8_t)(f.masked_payload[k] ^ f.key[k & 3]) == 'm');
    pos += f.total;

    BB_ASSERT(parse_client_frame(fs.rx + pos, fs.rx_len - pos, &f));
    BB_ASSERT((f.first & 0x0F) == 0x8 && f.masked);

    client_finish(&c);
    fs_stop(&fs);
    g_fs = NULL;
}

/* ============================================================
 * Test 3: failures during connect/handshake are reported, not hung on
 * ============================================================ */

static void script_close_immediately(fake_server_t *fs, bb_socket_t client, void *arg)
{
    (void)fs;
    (void)client;
    (void)arg;
}

static void script_garbage(fake_server_t *fs, bb_socket_t client, void *arg)
{
    (void)fs;
    (void)arg;
    const char *junk = "THIS IS NOT HTTP\r\n\r\n";
    fs_send(client, junk, strlen(junk));
    bb_usleep(100000);
}

static void script_partial_response(fake_server_t *fs, bb_socket_t client, void *arg)
{
    (void)fs;
    (void)arg;
    const char *part = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: webs";
    fs_send(client, part, strlen(part));
    bb_usleep(100000);
}

/* True if the client reported a failed connect (the only acceptable outcome). */
static bool failure_case_reported(const char *name, fake_script_fn script)
{
    fake_server_t fs;
    fs_start(&fs, script, NULL);
    g_fs = &fs;

    char url[128];
    make_url(url, sizeof(url), fs.port, "/chat");

    client_t c;
    client_start(&c, url);

    bool called = client_run_until(&c, pred_connected, 4);
    bool failed = called && BB_FAILED(c.connect_err);

    if (!called)
        fprintf(stderr, "\t  no connect callback after: %s (close callback fired %d time(s))\n", name, c.close_count);
    else if (!failed)
        fprintf(stderr, "\t  connect reported success after: %s\n", name);

    client_finish(&c);
    fs_stop(&fs);
    g_fs = NULL;
    return failed;
}

static void test_client_reports_connect_failures(void)
{
    printf("\tTesting client reports refused connections and malformed URLs...\n");

    /* Connection refused: grab a free port, then close it. */
    {
        fake_server_t fs;
        fs_start(&fs, NULL, NULL);
        int port = fs.port;
        fs_stop(&fs); /* thread times out waiting for a client; listener closed after */

        char url[128];
        make_url(url, sizeof(url), port, "/chat");

        client_t c;
        client_start(&c, url);
        BB_ASSERT(client_run_until(&c, pred_connected, 5));
        BB_ASSERT(BB_FAILED(c.connect_err));
        client_finish(&c);
    }

    /* Malformed URLs fail synchronously with an error and without touching the network. */
    static const char *const bad_urls[] = {
        "", "ws://", "ws:///path", "ws://host:99999/", "ws://host:0/", "ws://host:abc/",
        "http://127.0.0.1/", "ws://:80/",
    };
    for (size_t i = 0; i < sizeof(bad_urls) / sizeof(bad_urls[0]); i++)
    {
        client_t c;
        client_start(&c, bad_urls[i]);
        if (!c.connect_called || !BB_FAILED(c.connect_err))
            fprintf(stderr, "\t  malformed URL was not rejected: \"%s\"\n", bad_urls[i]);
        BB_ASSERT(c.connect_called);
        BB_ASSERT(BB_FAILED(c.connect_err));
        client_finish(&c);
    }

}

/* ============================================================
 * Strict RFC 6455 client conformance (known gaps; opt in)
 * ============================================================ */

static int strict_failures = 0;

#define STRICT_CHECK(name, cond)                                         \
    do                                                                   \
    {                                                                    \
        if (!(cond))                                                     \
        {                                                                \
            fprintf(stderr, "\t  STRICT RFC 6455 FAIL: %s\n", (name));   \
            strict_failures++;                                           \
        }                                                                \
    } while (0)

typedef enum
{
    BAD_STATUS_200,
    BAD_STATUS_404,
    BAD_WRONG_ACCEPT,
    BAD_MISSING_ACCEPT,
} bad_handshake_t;

static void script_bad_handshake(fake_server_t *fs, bb_socket_t client, void *arg)
{
    bad_handshake_t kind = (bad_handshake_t)(intptr_t)arg;
    char resp[512];

    switch (kind)
    {
    case BAD_STATUS_200:
    {
        const char *r = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nhi";
        fs_send(client, r, strlen(r));
        break;
    }
    case BAD_STATUS_404:
    {
        const char *r = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        fs_send(client, r, strlen(r));
        break;
    }
    case BAD_WRONG_ACCEPT:
    {
        size_t n = fs_build_101(fs, resp, sizeof(resp), true, "AAAAAAAAAAAAAAAAAAAAAAAAAAA=");
        fs_send(client, resp, n);
        break;
    }
    case BAD_MISSING_ACCEPT:
    {
        size_t n = fs_build_101(fs, resp, sizeof(resp), false, NULL);
        fs_send(client, resp, n);
        break;
    }
    }

    bb_usleep(300000);
}

static bool handshake_accepted(bad_handshake_t kind)
{
    fake_server_t fs;
    fs_start(&fs, script_bad_handshake, (void *)(intptr_t)kind);
    g_fs = &fs;

    char url[128];
    make_url(url, sizeof(url), fs.port, "/chat");

    client_t c;
    client_start(&c, url);
    (void)client_run_until(&c, pred_connected, 3);

    bool accepted = c.connect_called && !BB_FAILED(c.connect_err);

    client_finish(&c);
    fs_stop(&fs);
    g_fs = NULL;
    return accepted;
}

/* 101 immediately followed by frames in the same segment. */
static void script_eager_frame(fake_server_t *fs, bb_socket_t client, void *arg)
{
    (void)arg;
    char resp[512];
    uint8_t seg[600];
    size_t n = fs_build_101(fs, resp, sizeof(resp), true, NULL);
    memcpy(seg, resp, n);
    n += build_server_frame(seg + n, 0x81, "eager", 5);
    fs_send(client, seg, n);
    bb_usleep(300000);
}

/* A server frame with the MASK bit set (forbidden: RFC 6455 5.1). */
static void script_masked_server_frame(fake_server_t *fs, bb_socket_t client, void *arg)
{
    (void)arg;
    fs_send_101(fs, client);
    bb_usleep(50000);

    const uint8_t frame[] = {0x81, 0x83, 1, 2, 3, 4, 'a' ^ 1, 'b' ^ 2, 'c' ^ 3};
    fs_send(client, frame, sizeof(frame));
    bb_usleep(300000);
}

static bool pred_one_message(client_t *c) { return c->msg_count >= 1; }

static void script_accept_then_idle(fake_server_t *fs, bb_socket_t client, void *arg)
{
    (void)arg;
    fs_send_101(fs, client);
    bb_usleep(300000);
}

static void test_client_strict_rfc_conformance(void)
{
    if (getenv("BB_WS_STRICT_RFC") == NULL)
    {
        printf("\tSkipping strict RFC 6455 client conformance tests (set BB_WS_STRICT_RFC=1 to run).\n");
        return;
    }

    printf("\tTesting strict RFC 6455 client conformance (handshake validation, URL handling)...\n");
    strict_failures = 0;

    /* 4.1: the client MUST fail the connection unless the handshake response is a valid 101. */
    STRICT_CHECK("HTTP 200 reply to the upgrade is rejected", !handshake_accepted(BAD_STATUS_200));
    STRICT_CHECK("HTTP 404 reply to the upgrade is rejected", !handshake_accepted(BAD_STATUS_404));
    STRICT_CHECK("wrong Sec-WebSocket-Accept is rejected", !handshake_accepted(BAD_WRONG_ACCEPT));
    STRICT_CHECK("missing Sec-WebSocket-Accept is rejected", !handshake_accepted(BAD_MISSING_ACCEPT));

    /* 5.1: server frames sent right behind the 101 are legal and must not be lost. */
    {
        fake_server_t fs;
        fs_start(&fs, script_eager_frame, NULL);
        g_fs = &fs;
        char url[128];
        make_url(url, sizeof(url), fs.port, "/chat");

        client_t c;
        client_start(&c, url);
        (void)client_run_until(&c, pred_connected, 3);
        (void)client_run_until(&c, pred_one_message, 2);

        STRICT_CHECK("frame sent in the same segment as the 101 is delivered",
                     c.msg_count == 1 && c.msgs[0].len == 5 && memcmp(c.msgs[0].data, "eager", 5) == 0);

        client_finish(&c);
        fs_stop(&fs);
        g_fs = NULL;
    }

    /* 5.1: a masked server frame is a protocol error; the client must not deliver it. */
    {
        fake_server_t fs;
        fs_start(&fs, script_masked_server_frame, NULL);
        g_fs = &fs;
        char url[128];
        make_url(url, sizeof(url), fs.port, "/chat");

        client_t c;
        client_start(&c, url);
        (void)client_run_until(&c, pred_connected, 3);
        (void)client_run_until(&c, pred_one_message, 1);

        STRICT_CHECK("masked frame from the server is not delivered as a message", c.msg_count == 0);

        client_finish(&c);
        fs_stop(&fs);
        g_fs = NULL;
    }

    /* A URL must never be able to inject extra header lines into the handshake request. */
    {
        fake_server_t fs;
        fs_start(&fs, script_accept_then_idle, NULL);
        g_fs = &fs;

        char url[256];
        snprintf(url, sizeof(url), "ws://127.0.0.1:%d/chat\r\nInjected: yes", fs.port);

        client_t c;
        client_start(&c, url);
        (void)client_run_until(&c, pred_connected, 2);

        bool injected = ci_contains(fs.request, "\r\nInjected:");
        STRICT_CHECK("CR/LF in the URL cannot inject request headers", !injected);

        client_finish(&c);
        fs_stop(&fs);
        g_fs = NULL;
    }

    /* A long path is either rejected or sent completely; it must not be silently cut off. */
    {
        fake_server_t fs;
        fs_start(&fs, script_accept_then_idle, NULL);
        g_fs = &fs;

        char path[2200];
        path[0] = '/';
        memset(path + 1, 'p', 2000);
        path[2001] = '\0';

        char url[2400];
        snprintf(url, sizeof(url), "ws://127.0.0.1:%d%s", fs.port, path);

        client_t c;
        client_start(&c, url);
        (void)client_run_until(&c, pred_connected, 2);

        bool rejected = c.connect_called && BB_FAILED(c.connect_err);
        bool sent_whole = fs.accepted && strstr(fs.request, path) != NULL &&
                          strstr(fs.request, "\r\n\r\n") != NULL;
        STRICT_CHECK("a 2000-byte path is rejected or sent in full", rejected || sent_whole);

        client_finish(&c);
        fs_stop(&fs);
        g_fs = NULL;
    }

    /* A server that hangs up mid-handshake must be reported through the connect
     * callback, otherwise the caller waits for a connection that will never come. */
    STRICT_CHECK("server closing without answering is reported via the connect callback",
                 failure_case_reported("server closes without answering", script_close_immediately));
    STRICT_CHECK("half a handshake response followed by a close is reported via the connect callback",
                 failure_case_reported("server sends half a response then closes", script_partial_response));

    /* Keep this one last: after a handshake rejected as malformed, destroying the
     * client touches the already-freed connection (heap use-after-free), which
     * aborts the process under ASan. */
    STRICT_CHECK("client can be destroyed safely after a malformed handshake response",
                 failure_case_reported("server answers with non-HTTP bytes", script_garbage));

    BB_ASSERT(strict_failures == 0);
}

/* ============================================================ */

int main(void)
{
    /* The fake server uses raw sockets before any runtime exists. */
    BB_ASSERT(bb_platform_net_init() == 0);

    printf("Running WebSocket client pipeline tests...\n");

    test_client_frames_on_the_wire();
    test_client_handles_server_frames();
    test_client_reports_connect_failures();
    test_client_strict_rfc_conformance();

    bb_runtime_destroy(bb_runtime_default());
    printf("All tests passed.\n");
    return 0;
}
