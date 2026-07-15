#ifndef BB_JSON_H
#define BB_JSON_H

#ifdef __cplusplus
extern "C" {
#endif


#include <blue-bird/error/error.h>

#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    BB_JSON_NULL,
    BB_JSON_BOOL,
    BB_JSON_INT,
    BB_JSON_REAL,
    BB_JSON_TEXT,
    BB_JSON_ARRAY,
    BB_JSON_OBJECT,
    BB_JSON_NOT_INITIALIZED
} bb_json_node_type_t;

// Error Codes:
enum {
    BB_ERR_JSON_TYPE_MISMATCH = BB_ERR_UTILS + 100,
    BB_ERR_JSON_OVERFLOW,
};

typedef struct BBJsonNode bb_json_t;

bb_json_t *bb_json_create(bb_json_node_type_t type);
void bb_json_destroy(bb_json_t *json);
size_t bb_json_get_size(bb_json_t *json);
bb_json_node_type_t bb_json_get_type(bb_json_t *json);

// JSON Primitives
bb_error_t bb_json_set_value_bool(bb_json_t *json, bool value);
bb_error_t bb_json_set_value_integer(bb_json_t *json, int value);
bb_error_t bb_json_set_value_real(bb_json_t *json, float value);
bb_error_t bb_json_set_value_text(bb_json_t *json, const char *value);

bool bb_json_get_value_bool(bb_json_t *json);
int bb_json_get_value_integer(bb_json_t *json);
float bb_json_get_value_real(bb_json_t *json);
char *bb_json_get_value_text(bb_json_t *json);

// JSON Array
bb_error_t bb_json_array_push(bb_json_t *json_array, bb_json_t *element);
bb_json_t *bb_json_array_get_index(bb_json_t *json_array, unsigned int index);
bb_error_t bb_json_array_remove_at_index(bb_json_t *json_array, unsigned int index);

// JSON Object
bb_error_t bb_json_object_set_value(bb_json_t *json_object, const char *key, bb_json_t *value);
bb_json_t *bb_json_object_get_value(bb_json_t *json_object, const char *key);
bb_error_t bb_json_object_remove_key(bb_json_t *obj, const char *key);

// Serializer
int bb_json_serialize(bb_json_t *json, char **buffer, int *size);
int bb_json_serialize_indented(bb_json_t *json, char **buffer, int *size);

// Parser
int bb_json_parse(bb_json_t **json, char *buffer);

// Compare
int bb_json_compare(bb_json_t *json_a, bb_json_t *json_b); // 0 for equal, -1 for not equal

// File
int bb_json_load(bb_json_t **json, const char *path);
int bb_json_dump(bb_json_t *json, const char *path);

static inline bb_json_t *bb_json_new_null(void)
{
    return bb_json_create(BB_JSON_NULL);
}

static inline bb_json_t *bb_json_new_bool(bool v)
{
    bb_json_t *n = bb_json_create(BB_JSON_BOOL);
    bb_json_set_value_bool(n, v);
    return n;
}

static inline bb_json_t *bb_json_new_int(int v)
{
    bb_json_t *n = bb_json_create(BB_JSON_INT);
    bb_json_set_value_integer(n, v);
    return n;
}

static inline bb_json_t *bb_json_new_real(float v)
{
    bb_json_t *n = bb_json_create(BB_JSON_REAL);
    bb_json_set_value_real(n, v);
    return n;
}

static inline bb_json_t *bb_json_new_text(const char *v)
{
    bb_json_t *n = bb_json_create(BB_JSON_TEXT);
    bb_json_set_value_text(n, v);
    return n;
}

static inline bb_json_t *bb_json_new_array(void)
{
    bb_json_t *n = bb_json_create(BB_JSON_ARRAY);
    return n;
}

static inline bb_json_t *bb_json_new_object(void)
{
    bb_json_t *n = bb_json_create(BB_JSON_OBJECT);
    return n;
}

// DSL
#define BB_JSON(v) (v)

#define NULLV()   bb_json_new_null()
#define BOOL(v)   bb_json_new_bool(v)
#define INT(v)    bb_json_new_int(v)
#define REAL(v)   bb_json_new_real(v)
#define TEXT(v)   bb_json_new_text(v)


#define ARR(...)                                                        \
({                                                                      \
    bb_json_t *_arr = bb_json_create(BB_JSON_ARRAY);                    \
    bb_json_t *_items[] = { __VA_ARGS__ };                              \
    int _n = sizeof(_items) / sizeof(_items[0]);                        \
    for (int _i = 0; _i < _n; _i++)                                     \
        bb_json_array_push(_arr, _items[_i]);                           \
    _arr;                                                               \
})


typedef struct {
    const char *key;
    bb_json_t *value;
} json_kv_t;

#define KEY(k, v) ((json_kv_t){ (k), (v) })
#define OBJ(...)                                                        \
({                                                                      \
    bb_json_t *_obj = bb_json_create(BB_JSON_OBJECT);                   \
    json_kv_t _kvs[] = { __VA_ARGS__ };                                 \
    int _n = sizeof(_kvs) / sizeof(_kvs[0]);                            \
    for (int _i = 0; _i < _n; _i++)                                     \
        bb_json_object_set_value(_obj, _kvs[_i].key, _kvs[_i].value);   \
    _obj;                                                               \
})


#ifdef __cplusplus
}
#endif

#endif //BB_JSON_H
