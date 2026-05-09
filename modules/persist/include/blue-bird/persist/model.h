#ifndef BB_PERSIST_MODEL_H
#define BB_PERSIST_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif


#include "schema.h"

typedef struct BB_ModelHandle BB_ModelHandle;

typedef struct {
    const char *name;

    BB_ModelHandle *(*open)(const char *uri);
    void (*close)(BB_ModelHandle *h);

    int (*insert)(BB_ModelHandle *h, BB_Schema *schema, void *entity);
    int (*find_by_pk)(BB_ModelHandle *h, BB_Schema *schema, void *out, const void *key);
    int (*update)(BB_ModelHandle *h, BB_Schema *schema, void *entity);
    int (*remove)(BB_ModelHandle *h, BB_Schema *schema, const void *key);
    int (*find_all)(BB_ModelHandle *h, BB_Schema *schema, void **out_array, size_t *out_count);
    int (*find_first_by_field)(BB_ModelHandle *h, BB_Schema *schema, void *out, const char *field_name, const void *value);

} BB_ModelAPI;

// Registry:
int bb_model_register(const BB_ModelAPI *api);
const BB_ModelAPI *bb_model_get(const char *name);

//Defaults:
void bb_model_set_default(const char *name);
BB_ModelHandle *bb_model_open_default(const char *uri);


#ifdef __cplusplus
}
#endif

#endif
