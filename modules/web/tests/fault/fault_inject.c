#include "fault_inject.h"

#include <stdlib.h>
#include <string.h>

#include <blue-bird/runtime/runtime.h>

/* Matches the prototype runtime.h gets when bb_runtime_watch_fd_ex is renamed
 * to bb_fi_watch_fd_ex by fi_intercept.h. */
bb_task_t *bb_fi_watch_fd_ex(bb_runtime_t *runtime, bb_socket_t fd, int events, bb_watch_mode_t mode, const bb_task_config_t *config);

typedef struct
{
    unsigned calls;
    unsigned failures;
    unsigned fail_at; /* 0 = disarmed */
} site_t;

static site_t g_alloc;
static site_t g_watch;

static void arm(site_t *s, unsigned n)
{
    s->calls = 0;
    s->failures = 0;
    s->fail_at = n;
}

/* Counts the call and reports whether this is the one armed to fail. The
 * injector disarms itself after firing, so retry/recovery paths run normally. */
static int should_fail(site_t *s)
{
    s->calls++;
    if (s->fail_at != 0 && s->calls == s->fail_at)
    {
        s->failures++;
        s->fail_at = 0;
        return 1;
    }
    return 0;
}

void bb_fi_reset(void)
{
    arm(&g_alloc, 0);
    arm(&g_watch, 0);
}

void bb_fi_fail_alloc_at(unsigned n)  { arm(&g_alloc, n); }
void bb_fi_fail_watch_at(unsigned n)  { arm(&g_watch, n); }

unsigned bb_fi_alloc_calls(void)      { return g_alloc.calls; }
unsigned bb_fi_alloc_failures(void)   { return g_alloc.failures; }
unsigned bb_fi_watch_calls(void)      { return g_watch.calls; }
unsigned bb_fi_watch_failures(void)   { return g_watch.failures; }

void *bb_fi_malloc(size_t size)
{
    return should_fail(&g_alloc) ? NULL : malloc(size);
}

void *bb_fi_calloc(size_t n, size_t size)
{
    return should_fail(&g_alloc) ? NULL : calloc(n, size);
}

void *bb_fi_realloc(void *ptr, size_t size)
{
    /* A failed realloc leaves the original block untouched, as the real one does. */
    return should_fail(&g_alloc) ? NULL : realloc(ptr, size);
}

char *bb_fi_strdup(const char *s)
{
    if (should_fail(&g_alloc))
    {
        return NULL;
    }
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy)
    {
        memcpy(copy, s, len);
    }
    return copy;
}

bb_task_t *bb_fi_watch_fd_ex(bb_runtime_t *runtime, bb_socket_t fd, int events, bb_watch_mode_t mode, const bb_task_config_t *config)
{
    if (should_fail(&g_watch))
    {
        return NULL;
    }
    return bb_runtime_watch_fd_ex(runtime, fd, events, mode, config);
}
