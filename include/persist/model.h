#ifndef BB_PERSIST_MODEL_H
#define BB_PERSIST_MODEL_H

#include "schema.h"

typedef struct BB_ModelHandle BB_ModelHandle;

typedef struct {
    const char *name;

    BB_ModelHandle *(*open)(const char *uri);
    void (*close)(BB_ModelHandle *h);

    int (*insert)(BB_ModelHandle *h, BB_Schema *schema, void *entity);
    int (*find_by_id)(BB_ModelHandle *h, BB_Schema *schema, void *out, int id);
    int (*update)(BB_ModelHandle *h, BB_Schema *schema, void *entity);
    int (*remove)(BB_ModelHandle *h, BB_Schema *schema, int id);

} BB_ModelAPI;

// Registry:
int bb_model_register(const BB_ModelAPI *api);
const BB_ModelAPI *bb_model_get(const char *name);

//Defaults:
void bb_model_set_default(const char *name);
BB_ModelHandle *bb_model_open_default(const char *uri);

#endif
