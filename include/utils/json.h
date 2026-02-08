#ifndef BB_JSON
#define BB_JSON

#include <stdbool.h>
#include <stdlib.h>

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_INT,
    JSON_REAL,
    JSON_TEXT,
    JSON_ARRAY,
    JSON_OBJECT
} json_node_type;

typedef struct JsonNode {
    json_node_type type;
    int size;       // For array type
    int alloc_size; // allocated size for array
    union {
        bool bool_val;
        int int_val;
        float real_val;
        char *text_val;

        struct JsonNode **array;
    } value;
    // key array for proof of concept.
    // the array above is used for values.
    // Hash Table will be implemented.
    char **key;
} json_node_t;

void init_json(json_node_t *json, json_node_type type);
void destroy_json(json_node_t *json);

// JSON Primitives
void set_json_bool_value(json_node_t *json, bool value);
void set_json_integer_value(json_node_t *json, int value);
void set_json_real_value(json_node_t *json, float value);
void set_json_text_value(json_node_t *json, const char *value);

bool get_json_bool_value(json_node_t *json);
int get_json_integer_value(json_node_t *json);
float get_json_real_value(json_node_t *json);
char *get_json_text_value(json_node_t *json);

// JSON Array
void push_json_array(json_node_t *json_array, json_node_t *element);
json_node_t *get_json_array_index(json_node_t *json_array, unsigned int index);

// JSON Object
void set_json_object_value(json_node_t *json_object, const char *key, json_node_t *value);
json_node_t *get_json_object_value(json_node_t *json_object, const char *key);

// Serializer
int serialize_json(json_node_t *json, char **buffer, int *size);
int indented_serialize_json(json_node_t *json, char **buffer, int *size);

// Parser
int parse_json_str(json_node_t *json, char *buffer);

// File
int load_json(json_node_t *json, const char *path);
int dump_json(json_node_t *json, const char *path);

static inline json_node_t *json_new(json_node_type type)
{
    json_node_t *n = malloc(sizeof(json_node_t));
    init_json(n, type);
    return n;
}

static inline json_node_t *json_new_null(void)
{
    return json_new(JSON_NULL);
}

static inline json_node_t *json_new_bool(bool v)
{
    json_node_t *n = json_new(JSON_BOOL);
    set_json_bool_value(n, v);
    return n;
}

static inline json_node_t *json_new_int(int v)
{
    json_node_t *n = json_new(JSON_INT);
    set_json_integer_value(n, v);
    return n;
}

static inline json_node_t *json_new_real(float v)
{
    json_node_t *n = json_new(JSON_REAL);
    set_json_real_value(n, v);
    return n;
}

static inline json_node_t *json_new_text(const char *v)
{
    json_node_t *n = json_new(JSON_TEXT);
    set_json_text_value(n, v);
    return n;
}

// Macros
#define JSON_NULL_NODE()      ((json_node_t){ .type = JSON_NULL })
#define JSON_BOOL(v)          ((json_node_t){ .type = JSON_BOOL, .value.bool_val = (v) })
#define JSON_INT(v)           ((json_node_t){ .type = JSON_INT,  .value.int_val  = (v) })
#define JSON_REAL(v)          ((json_node_t){ .type = JSON_REAL, .value.real_val = (v) })
#define JSON_TEXT(v)          ((json_node_t){ .type = JSON_TEXT, .value.text_val = (char *)(v) })


#define JSON_NEW(type) \
    ({ json_node_t *_n = malloc(sizeof(json_node_t)); init_json(_n, (type)); _n; })

#define JSON_NEW_INT(v)   ({ json_node_t *_n = JSON_NEW(JSON_INT);  set_json_integer_value(_n, (v)); _n; })
#define JSON_NEW_BOOL(v)  ({ json_node_t *_n = JSON_NEW(JSON_BOOL); set_json_bool_value(_n, (v)); _n; })
#define JSON_NEW_REAL(v)  ({ json_node_t *_n = JSON_NEW(JSON_REAL); set_json_real_value(_n, (v)); _n; })
#define JSON_NEW_TEXT(v)  ({ json_node_t *_n = JSON_NEW(JSON_TEXT); set_json_text_value(_n, (v)); _n; })


#define JSON_OBJECT_BEGIN(name) \
    json_node_t *name = JSON_NEW(JSON_OBJECT)

#define JSON_OBJECT_FIELD(obj, key, value) \
    set_json_object_value((obj), (key), (value))


// DSL
#define JSON(v) (v)

#define NULLV()   json_new_null()
#define BOOL(v)   json_new_bool(v)
#define INT(v)    json_new_int(v)
#define REAL(v)   json_new_real(v)
#define TEXT(v)   json_new_text(v)


#define ARR(...)                                                   \
({                                                                 \
    json_node_t *_arr = json_new(JSON_ARRAY);                      \
    json_node_t *_items[] = { __VA_ARGS__ };                       \
    int _n = sizeof(_items) / sizeof(_items[0]);                   \
    for (int _i = 0; _i < _n; _i++)                                \
        push_json_array(_arr, _items[_i]);                         \
    _arr;                                                          \
})


typedef struct {
    const char *key;
    json_node_t *value;
} json_kv_t;

#define KEY(k, v) ((json_kv_t){ (k), (v) })
#define OBJ(...)                                                    \
({                                                                  \
    json_node_t *_obj = json_new(JSON_OBJECT);                      \
    json_kv_t _kvs[] = { __VA_ARGS__ };                             \
    int _n = sizeof(_kvs) / sizeof(_kvs[0]);                        \
    for (int _i = 0; _i < _n; _i++)                                 \
        set_json_object_value(_obj, _kvs[_i].key, _kvs[_i].value);  \
    _obj;                                                           \
})

#endif //BB_JSON
