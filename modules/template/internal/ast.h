#ifndef BB_TEMPLATE_AST_H
#define BB_TEMPLATE_AST_H

#include <stddef.h>

typedef enum {
    BB_TEMPLATE_NODE_TEXT,
    BB_TEMPLATE_NODE_VARIABLE
} bb_template_node_type_t;

typedef struct bb_template_node {
    bb_template_node_type_t type;
    char *value;
    struct bb_template_node *next;
} bb_template_node_t;

typedef struct {
    bb_template_node_t *head;
    bb_template_node_t *tail;
} bb_template_ast_t;

bb_template_node_t *bb_template_node_create(bb_template_node_type_t type, const char *value);
void bb_template_ast_append(bb_template_ast_t *ast, bb_template_node_t *node);
void bb_template_ast_destroy(bb_template_ast_t *ast);

#endif
