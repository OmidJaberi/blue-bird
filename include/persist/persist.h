#ifndef BLUE_BIRD_PERSIST_H
#define BLUE_BIRD_PERSIST_H

#include <stddef.h>

typedef struct PersistHandle PersistHandle;

typedef struct PersistAPI {
    const char *name;  /**< Name of the backend, e.g. "file" or "sqlite" */

    PersistHandle *(*open)(const char *uri);
    void (*close)(PersistHandle *handle);

    int (*save)(PersistHandle *handle,
                const char *key,
                const void *data,
                size_t size);

    int (*load)(PersistHandle *handle,
                const char *key,
                void *buf,
                size_t bufsize);

    int (*remove)(PersistHandle *handle,
                  const char *key);
} PersistAPI;

int persist_register(const PersistAPI *api);

const PersistAPI *persist_get(const char *name);

int persist_list(const char **out, int max);

void persist_set_default(const char *name);

const char *persist_get_default(void);

PersistHandle *persist_open_default(const char *uri);

int persist_save(const char *key, const void *data, size_t size);

int persist_load(const char *key, void *buf, size_t bufsize);

int persist_remove(const char *key);

int persist_init(void);

void persist_shutdown(void);

#endif /* BLUE_BIRD_PERSIST_H */

