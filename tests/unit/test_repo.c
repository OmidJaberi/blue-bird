#include "repo/repo.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ============================================================
   Minimal in-memory persist backend (TEST ONLY)
   ============================================================ */

/*
 * We override persist_save/load/remove here so repo.c
 * links against these instead of any real backend.
 */

typedef struct {
    char   *key;
    void   *data;
    size_t  size;
} entry_t;

static entry_t *g_entries = NULL;
static size_t   g_count   = 0;

static void mem_reset(void)
{
    for (size_t i = 0; i < g_count; i++) {
        free(g_entries[i].key);
        free(g_entries[i].data);
    }
    free(g_entries);
    g_entries = NULL;
    g_count = 0;
}

int persist_save(const char *key, const void *data, size_t size)
{
    /* overwrite if exists */
    for (size_t i = 0; i < g_count; i++) {
        if (strcmp(g_entries[i].key, key) == 0) {
            free(g_entries[i].data);
            g_entries[i].data = malloc(size);
            memcpy(g_entries[i].data, data, size);
            g_entries[i].size = size;
            return 0;
        }
    }

    entry_t *tmp = realloc(g_entries, sizeof(entry_t) * (g_count + 1));
    if (!tmp)
        return -1;

    g_entries = tmp;

    g_entries[g_count].key = strdup(key);
    g_entries[g_count].data = malloc(size);
    memcpy(g_entries[g_count].data, data, size);
    g_entries[g_count].size = size;

    g_count++;
    return 0;
}

int persist_load(const char *key, void *buf, size_t bufsize)
{
    for (size_t i = 0; i < g_count; i++) {
        if (strcmp(g_entries[i].key, key) == 0) {
            if (bufsize < g_entries[i].size)
                return -1;

            memcpy(buf, g_entries[i].data, g_entries[i].size);
            return (int)g_entries[i].size;
        }
    }

    return -1;
}

int persist_remove(const char *key)
{
    for (size_t i = 0; i < g_count; i++) {
        if (strcmp(g_entries[i].key, key) == 0) {
            free(g_entries[i].key);
            free(g_entries[i].data);

            for (size_t j = i; j < g_count - 1; j++)
                g_entries[j] = g_entries[j + 1];

            g_count--;
            return 0;
        }
    }

    return -1;
}

/* ============================================================
   Test Record + Encode/Decode
   ============================================================ */

typedef struct {
    uint64_t id;
    char name[64];
} user_t;

static int encode_user(
    const void *record,
    void **out_buf,
    size_t *out_size
)
{
    *out_size = sizeof(user_t);
    *out_buf = malloc(*out_size);
    if (!*out_buf)
        return -1;

    memcpy(*out_buf, record, *out_size);
    return 0;
}

static int decode_user(
    const void *buf,
    size_t size,
    void *out_record
)
{
    if (size != sizeof(user_t))
        return -1;

    memcpy(out_record, buf, size);
    return 0;
}

/* ============================================================
   Tests
   ============================================================ */

static void test_repo_create(void)
{
    bb_repo_t *repo =
        bb_repo_create("users", encode_user, decode_user);

    assert(repo != NULL);
    bb_repo_destroy(repo);

    assert(bb_repo_create(NULL, encode_user, decode_user) == NULL);
    assert(bb_repo_create("x", NULL, decode_user) == NULL);
    assert(bb_repo_create("x", encode_user, NULL) == NULL);
}

static void test_repo_put_get(void)
{
    mem_reset();

    bb_repo_t *repo =
        bb_repo_create("users", encode_user, decode_user);

    assert(repo);

    user_t in = { .id = 1 };
    snprintf(in.name, sizeof in.name, "alice");

    assert(bb_repo_put(repo, 1, &in) == BB_REPO_OK);

    user_t out = {0};
    assert(bb_repo_get(repo, 1, &out) == BB_REPO_OK);

    assert(out.id == 1);
    assert(strcmp(out.name, "alice") == 0);

    bb_repo_destroy(repo);
}

static void test_repo_overwrite(void)
{
    mem_reset();

    bb_repo_t *repo =
        bb_repo_create("users", encode_user, decode_user);

    user_t u1 = { .id = 42 };
    snprintf(u1.name, sizeof u1.name, "first");

    user_t u2 = { .id = 42 };
    snprintf(u2.name, sizeof u2.name, "second");

    assert(bb_repo_put(repo, 42, &u1) == BB_REPO_OK);
    assert(bb_repo_put(repo, 42, &u2) == BB_REPO_OK);

    user_t out = {0};
    assert(bb_repo_get(repo, 42, &out) == BB_REPO_OK);
    assert(strcmp(out.name, "second") == 0);

    bb_repo_destroy(repo);
}

static void test_repo_get_missing(void)
{
    mem_reset();

    bb_repo_t *repo =
        bb_repo_create("users", encode_user, decode_user);

    user_t out;
    assert(bb_repo_get(repo, 999, &out) == BB_REPO_NOT_FOUND);

    bb_repo_destroy(repo);
}

static void test_repo_delete(void)
{
    mem_reset();

    bb_repo_t *repo =
        bb_repo_create("users", encode_user, decode_user);

    user_t u = { .id = 5 };
    snprintf(u.name, sizeof u.name, "bob");

    assert(bb_repo_put(repo, 5, &u) == BB_REPO_OK);
    assert(bb_repo_delete(repo, 5) == BB_REPO_OK);

    user_t out;
    assert(bb_repo_get(repo, 5, &out) == BB_REPO_NOT_FOUND);

    bb_repo_destroy(repo);
}

static void test_repo_delete_missing(void)
{
    mem_reset();

    bb_repo_t *repo =
        bb_repo_create("users", encode_user, decode_user);

    assert(bb_repo_delete(repo, 1234) == BB_REPO_NOT_FOUND);

    bb_repo_destroy(repo);
}

/* ============================================================
   Main
   ============================================================ */

int main(void)
{
    test_repo_create();
    test_repo_put_get();
    test_repo_overwrite();
    test_repo_get_missing();
    test_repo_delete();
    test_repo_delete_missing();

    printf("All repo tests passed.\n");
    return 0;
}
