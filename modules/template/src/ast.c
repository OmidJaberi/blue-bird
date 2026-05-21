#include "ast.h"

#include <stdlib.h>
#include <string.h>

bb_template_node_t *bb_template_node_create(bb_template_node_type_t type, const char *value)
{
    bb_template_node_t *node = malloc(sizeof(bb_template_node_t));

    if (!node)
    {
        return NULL;
    }

    node->type = type;
    node->next = NULL;

    node->value = strdup(value ? value : "");

    if (!node->value)
    {
        free(node);
        return NULL;
    }

    return node;
}

void bb_template_ast_append(bb_template_ast_t *ast, bb_template_node_t *node)
{
    if (!ast->head)
    {
        ast->head = node;
        ast->tail = node;
        return;
    }

    ast->tail->next = node;
    ast->tail = node;
}

void bb_template_ast_destroy(bb_template_ast_t *ast)
{
    bb_template_node_t *node = ast->head;

    while (node)
    {
        bb_template_node_t *next = node->next;

        free(node->value);
        free(node);

        node = next;
    }

    ast->head = NULL;
    ast->tail = NULL;
}
