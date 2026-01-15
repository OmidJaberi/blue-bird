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
    int size; // For array type

    union {
        bool bool_val;
        int int_val;
        float real_val;
        char *text_val;
        json_node_t *array;
        // map for object?
    } value;
} json_node_t;

#endif //BB_JSON
