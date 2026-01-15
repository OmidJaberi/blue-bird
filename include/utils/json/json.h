#ifndef BB_JSON
#define BB_JSON

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
} json_node_t;

#endif //BB_JSON
