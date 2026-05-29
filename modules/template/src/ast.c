#include "ast.h"

#include <stdlib.h>
#include <string.h>

bb_template_node_t *bb_template_node_create(bb_template_node_type_t type, const char *value)
{
    bb_template_node_t *node = calloc(1, sizeof(*node));

    if (!node)
    {
        return NULL;
    }

    node->type = type;

    node->value = strdup(value ? value : "");

    if (!node->value)
    {
        free(node);
        return NULL;
    }

    return node;
}

void bb_template_node_append_child(bb_template_node_t *parent, bb_template_node_t *child)
{
    if (!parent || !child)
    {
        return;
    }

    // First child.
    if (!parent->children)
    {
        parent->children = child;
        return;
    }

    // Append to sibling chain.
    bb_template_node_t *current = parent->children;
    while (current->next)
    {
        current = current->next;
    }
    current->next = child;
}

void bb_template_node_list_append(bb_template_node_list_t *list, bb_template_node_t *node)
{
    if (!list || !node)
    {
        return;
    }
    node->next_list = NULL;

    if (!list->head)
    {
        list->head = list->tail = node;
        return;
    }

    list->tail->next_list = node;
    list->tail = node;
}

void bb_template_node_destroy(bb_template_node_t *node)
{
    if (!node)
    {
        return;
    }

    // Destroy children recursively.
    bb_template_node_t *child = node->children;

    while (child)
    {
        bb_template_node_t *next = child->next;
        bb_template_node_destroy(child);
        child = next;
    }

    free(node->value);
    free(node);
}

void bb_template_node_list_destroy(bb_template_node_list_t *list)
{
    if (!list)
    {
        return;
    }

    bb_template_node_t *node = list->head;
    while (node)
    {
        bb_template_node_t *next = node->next_list;
        bb_template_node_destroy(node);
        node = next;
    }

    list->head = NULL;
    list->tail = NULL;
}
