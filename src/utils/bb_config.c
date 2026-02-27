#include "utils/bb_config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define INITIAL_CAPACITY 16

/* ---------- Internal ---------- */

typedef struct {
    char *key;
    char *value;
} bb_config_pair_t;

struct bb_config {
    bb_config_pair_t *pairs;
    size_t count;
    size_t capacity;
};

static int ensure_capacity(bb_config_t *cfg)
{
    if (cfg->count < cfg->capacity)
        return 0;

    size_t newcap = cfg->capacity * 2;
    bb_config_pair_t *newpairs =
        realloc(cfg->pairs, newcap * sizeof(bb_config_pair_t));
    if (!newpairs)
        return 1;

    cfg->pairs = newpairs;
    cfg->capacity = newcap;
    return 0;
}

static int find_index(bb_config_t *cfg, const char *key)
{
    for (size_t i = 0; i < cfg->count; i++)
    {
        if (strcmp(cfg->pairs[i].key, key) == 0)
            return (int)i;
    }
    return -1;
}

bb_config_t *bb_config_new(void)
{
    bb_config_t *cfg = malloc(sizeof(bb_config_t));
    if (!cfg)
        return NULL;

    cfg->capacity = INITIAL_CAPACITY;
    cfg->count = 0;
    cfg->pairs = calloc(cfg->capacity, sizeof(bb_config_pair_t));

    if (!cfg->pairs)
    {
        free(cfg);
        return NULL;
    }

    return cfg;
}

void bb_config_free(bb_config_t *cfg)
{
    if (!cfg)
        return;

    for (size_t i = 0; i < cfg->count; i++)
    {
        free(cfg->pairs[i].key);
        free(cfg->pairs[i].value);
    }

    free(cfg->pairs);
    free(cfg);
}

int bb_config_set(bb_config_t *cfg,
                  const char *key,
                  const char *value)
{
    if (!cfg || !key || !value)
        return 1;

    int idx = find_index(cfg, key);

    if (idx >= 0)
    {
        free(cfg->pairs[idx].value);
        cfg->pairs[idx].value = strdup(value);
        return 0;
    }

    if (ensure_capacity(cfg) != 0)
        return 1;

    cfg->pairs[cfg->count].key = strdup(key);
    cfg->pairs[cfg->count].value = strdup(value);

    if (!cfg->pairs[cfg->count].key ||
        !cfg->pairs[cfg->count].value)
        return 1;

    cfg->count++;
    return 0;
}

const char *bb_config_get(bb_config_t *cfg,
                          const char *key)
{
    if (!cfg || !key)
        return NULL;

    int idx = find_index(cfg, key);
    return (idx >= 0) ? cfg->pairs[idx].value : NULL;
}

const char *bb_config_get_default(bb_config_t *cfg,
                                  const char *key,
                                  const char *def)
{
    const char *v = bb_config_get(cfg, key);
    return v ? v : def;
}

int bb_config_get_int(bb_config_t *cfg,
                      const char *key,
                      int def)
{
    const char *v = bb_config_get(cfg, key);
    return v ? atoi(v) : def;
}

int bb_config_get_bool(bb_config_t *cfg,
                       const char *key,
                       int def)
{
    const char *v = bb_config_get(cfg, key);
    if (!v)
        return def;

    if (strcasecmp(v, "true") == 0 ||
        strcmp(v, "1") == 0)
        return 1;

    if (strcasecmp(v, "false") == 0 ||
        strcmp(v, "0") == 0)
        return 0;

    return def;
}