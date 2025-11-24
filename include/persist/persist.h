#ifndef BLUE_BIRD_PERSIST_H
#define BLUE_BIRD_PERSIST_H

#include <stddef.h>

typedef struct PersistHandle PersistHandle;

/* Backends expose this, and register once */
typedef struct PersistAPI {
    const char *name;

    PersistHandle *(*open)(const char *uri);
    void (*close)(PersistHandle *h);

    int (*save)(PersistHandle *h, const char *key,
                const void *data, size_t size);

    int (*load)(PersistHandle *h, const char *key,
                void *buf, size_t bufsize);

    int (*remove)(PersistHandle *h, const char *key);
} PersistAPI;

/* registry */
int persist_register(const PersistAPI *api);
const PersistAPI *persist_get(const char *name);

/* default backend */
void persist_set_default(const char *name);
const char *persist_get_default(void);
PersistHandle *persist_open_default(const char *uri);

/* simple wrappers using default backend */
int persist_save(const char *key, const void *data, size_t size);
int persist_load(const char *key, void *buf, size_t bufsize);
int persist_remove(const char *key);

#endif /* BLUE_BIRD_PERSIST_H */

