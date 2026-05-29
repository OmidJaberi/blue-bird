#ifndef BB_TEMPLATE_AST_H
#define BB_TEMPLATE_AST_H

#include <stddef.h>

typedef enum {
    BB_TEMPLATE_NODE_TEXT,
    BB_TEMPLATE_NODE_VARIABLE,
    BB_TEMPLATE_NODE_SECTION,
    BB_TEMPLATE_NODE_CONDITIONAL,
    BB_TEMPLATE_NODE_COMMENT
} bb_template_node_type_t;

typedef struct bb_template_node {
    bb_template_node_type_t type;

    /*
     * Variable name,
     * section name,
     * conditional name,
     * or text contents.
     */
    char *value;

    /*
     * Child nodes for:
     * - sections
     * - conditionals
     */
    struct bb_template_node *children;

    /*
     * Sibling node.
     */
    struct bb_template_node *next;

    /*
    * List linkage.
    */
    struct bb_template_node *next_list;
} bb_template_node_t;

typedef struct {
    bb_template_node_t *head;
    bb_template_node_t *tail;

} bb_template_node_list_t;

bb_template_node_t *bb_template_node_create( bb_template_node_type_t type, const char *value);
void bb_template_node_append_child(bb_template_node_t *parent, bb_template_node_t *child);
void bb_template_node_list_append(bb_template_node_list_t *list, bb_template_node_t *node);
void bb_template_node_destroy(bb_template_node_t *node);
void bb_template_node_list_destroy(bb_template_node_list_t *list);

#endif
