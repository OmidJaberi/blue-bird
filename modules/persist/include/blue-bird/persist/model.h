#ifndef BB_PERSIST_MODEL_H
#define BB_PERSIST_MODEL_H

#ifdef __cplusplus
extern "C" {
#endif


#include "schema.h"

typedef struct bb_model_handle_t bb_model_handle_t;

typedef struct {
    const char *name;

    bb_model_handle_t *(*open)(const char *uri);
    void (*close)(bb_model_handle_t *h);

    int (*insert)(bb_model_handle_t *h, bb_schema_t *schema, void *entity);
    int (*find_by_pk)(bb_model_handle_t *h, bb_schema_t *schema, void *out, const void *key);
    int (*update)(bb_model_handle_t *h, bb_schema_t *schema, void *entity);
    int (*remove)(bb_model_handle_t *h, bb_schema_t *schema, const void *key);
    int (*find_all)(bb_model_handle_t *h, bb_schema_t *schema, void **out_array, size_t *out_count);
    int (*find_first_by_field)(bb_model_handle_t *h, bb_schema_t *schema, void *out, const char *field_name, const void *value);

} bb_model_api_t;

// Registry:
int bb_model_register(const bb_model_api_t *api);
const bb_model_api_t *bb_model_get(const char *name);

//Defaults:
void bb_model_set_default(const char *name);
bb_model_handle_t *bb_model_open_default(const char *uri);


#ifdef __cplusplus
}
#endif

#endif
