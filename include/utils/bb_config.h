#ifndef BB_UTILS_CONFIG_H
#define BB_UTILS_CONFIG_H

#include <stddef.h>

typedef struct bb_config bb_config_t;

/* ---------- Lifecycle ---------- */

bb_config_t *bb_config_new(void);
void bb_config_free(bb_config_t *cfg);

/* ---------- Loaders ---------- */

/* Load KEY=VALUE .env file */
int bb_config_load_env(bb_config_t *cfg, const char *path);

/* Load JSON file (flat object only) */
int bb_config_load_json(bb_config_t *cfg, const char *path);

/* ---------- Setters ---------- */

int bb_config_set(bb_config_t *cfg,
                  const char *key,
                  const char *value);

/* ---------- Getters ---------- */

const char *bb_config_get(bb_config_t *cfg,
                          const char *key);

const char *bb_config_get_default(bb_config_t *cfg,
                                  const char *key,
                                  const char *def);

int bb_config_get_int(bb_config_t *cfg,
                      const char *key,
                      int def);

int bb_config_get_bool(bb_config_t *cfg,
                       const char *key,
                       int def);

#endif