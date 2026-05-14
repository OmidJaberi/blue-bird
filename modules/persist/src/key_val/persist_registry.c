#include <string.h>
#include "blue-bird/persist/key_val.h"

#define MAX_BACKENDS 8

static const bb_persist_kv_api_t *g_backends[MAX_BACKENDS];
static int g_backend_count = 0;

static const bb_persist_kv_api_t *g_default = NULL;
static char g_default_uri[256] = {0};

/* -------------------------------------------------------
 * Registration
 * ----------------------------------------------------- */

int bb_persist_kv_register(const bb_persist_kv_api_t *api)
{
    if (!api || g_backend_count >= MAX_BACKENDS)
        return 1;

    g_backends[g_backend_count++] = api;
    return 0;
}

const bb_persist_kv_api_t *persist_get(const char *name)
{
    for (int i = 0; i < g_backend_count; i++) {
        if (strcmp(g_backends[i]->name, name) == 0)
            return g_backends[i];
    }
    return NULL;
}

int persist_list(const char **out, int max)
{
    int n = (g_backend_count < max) ? g_backend_count : max;
    for (int i = 0; i < n; i++)
        out[i] = g_backends[i]->name;
    return n;
}

void bb_persist_kv_set_default(const char *name)
{
    g_default = persist_get(name);
}

const char *bb_persist_kv_get_default(void)
{
    return g_default ? g_default->name : NULL;
}

void bb_persist_kv_set_default_uri(const char *uri)
{
    if (!uri) return;
    strncpy(g_default_uri, uri, sizeof(g_default_uri)-1);
}

/* -------------------------------------------------------
 * Wrapper API
 * ----------------------------------------------------- */

int bb_persist_kv_save(const char *key, const void *data, size_t size)
{
    if (!g_default || g_default_uri[0] == '\0')
        return 1;

    bb_persist_kv_handle_t *h = g_default->open(g_default_uri);
    if (!h) return 1;

    int rc = g_default->save(h, key, data, size);
    g_default->close(h);

    return rc;
}

int bb_persist_kv_load(const char *key, void *buf, size_t bufsize)
{
    if (!g_default || g_default_uri[0] == '\0')
        return 1;

    bb_persist_kv_handle_t *h = g_default->open(g_default_uri);
    if (!h) return 1;

    int rc = g_default->load(h, key, buf, bufsize);
    g_default->close(h);

    return rc;
}

int bb_persist_kv_remove(const char *key)
{
    if (!g_default || g_default_uri[0] == '\0')
        return 1;

    bb_persist_kv_handle_t *h = g_default->open(g_default_uri);
    if (!h) return 1;

    int rc = g_default->remove(h, key);
    g_default->close(h);

    return rc;
}
