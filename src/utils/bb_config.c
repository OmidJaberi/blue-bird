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