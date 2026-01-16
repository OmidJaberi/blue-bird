#ifndef BB_JSON
#define BB_JSON

#include <stdbool.h>

typedef enum {
    null,
    boolean,
    integer,
    real,
    text,
    array,
    object
} json_node_type;

typedef struct {
    json_node_type type;
    int size;       // For array type
    int alloc_size; // allocated size for array
    union {
        bool bool_val;
        int int_val;
        float real_val;
        char *text_val;

        struct json_node_t *array;
        // key-value array for proof of concept.
        // Hash Table will be implemented.
        struct {
            char *key;
            struct json_node_t *value;
        } *json_obj_arr;
    } value;
} json_node_t;

void init_json(json_node_t *json, json_node_type type);
void destroy_json(json_node_t *json);
json_node_type get_json_type(json_node_t *json);

void set_json_bool_value(json_node_t *json, bool value);
void set_json_integer_value(json_node_t *json, int value);
void set_json_real_value(json_node_t *json, float value);
void set_json_text_value(json_node_t *json, const char *value);

bool get_json_bool_value(json_node_t *json);
int get_json_integer_value(json_node_t *json);
float get_json_real_value(json_node_t *json);
char *get_json_text_value(json_node_t *json);

#endif //BB_JSON
