#ifndef BB_KEY_VAL_PERSIST_H
#define BB_KEY_VAL_PERSIST_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

typedef struct bb_persist_kv_handle_t bb_persist_kv_handle_t;

/* Backends expose this, and register once */
typedef struct {
    const char *name;

    bb_persist_kv_handle_t *(*open)(const char *uri);
    void (*close)(bb_persist_kv_handle_t *h);

    int (*save)(bb_persist_kv_handle_t *h, const char *key,
                const void *data, size_t size);

    int (*load)(bb_persist_kv_handle_t *h, const char *key,
                void *buf, size_t bufsize);

    int (*remove)(bb_persist_kv_handle_t *h, const char *key);
} bb_persist_kv_api_t;

/* registry */
int bb_persist_kv_register(const bb_persist_kv_api_t *api);
const bb_persist_kv_api_t *persist_get(const char *name);

/* default backend */
void bb_persist_kv_set_default(const char *name);
void bb_persist_kv_set_default_uri(const char *uri);
const char *bb_persist_kv_get_default(void);
bb_persist_kv_handle_t *bb_persist_kv_open_default(const char *uri);

/* simple wrappers using default backend */
int bb_persist_kv_save(const char *key, const void *data, size_t size);
int bb_persist_kv_load(const char *key, void *buf, size_t bufsize);
int bb_persist_kv_remove(const char *key);


#ifdef __cplusplus
}
#endif

#endif //BB_KEY_VAL_PERSIST_H
