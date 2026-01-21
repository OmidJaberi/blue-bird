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
json_node_type get_json_type(json_node_t *json);

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
int serialize_json(json_node_t *json, char *buffer);

// Parser
int parse_json_str(json_node_t *json, char *buffer);

#endif //BB_JSON
