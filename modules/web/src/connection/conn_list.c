#include "connection/conn_list.h"

#include <stdlib.h>

struct bb_conn_node {
    void *data;
    bb_conn_node_t *prev;
    bb_conn_node_t *next;
};

struct bb_conn_list {
    bb_conn_node_t *head;
    bb_conn_node_t *tail;
};

bb_conn_list_t *bb_conn_list_create(void)
{
    return calloc(1, sizeof(bb_conn_list_t));
}

bb_conn_node_t *bb_conn_list_add(bb_conn_list_t *list, void *data)
{
    if (!list)
    {
        return NULL;
    }

    bb_conn_node_t *node = malloc(sizeof(*node));
    if (!node)
    {
        return NULL;
    }

    node->data = data;
    node->prev = list->tail;
    node->next = NULL;

    if (list->tail)
    {
        list->tail->next = node;
    }
    else
    {
        list->head = node;
    }
    list->tail = node;

    return node;
}

void bb_conn_list_remove(bb_conn_list_t *list, bb_conn_node_t *node)
{
    if (!list || !node)
    {
        return;
    }

    if (node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        list->head = node->next;
    }

    if (node->next)
    {
        node->next->prev = node->prev;
    }
    else
    {
        list->tail = node->prev;
    }

    free(node);
}

void bb_conn_list_destroy_all(bb_conn_list_t *list, bb_conn_cleanup_fn cleanup)
{
    if (!list)
    {
        return;
    }

    bb_conn_node_t *node = list->head;
    while (node)
    {
        bb_conn_node_t *next = node->next;
        if (cleanup)
        {
            cleanup(node->data);
        }
        free(node);
        node = next;
    }

    free(list);
}
