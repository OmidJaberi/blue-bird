#include "websocket/websocket_list.h"

bb_ws_list_t *bb_ws_list_create(void)
{
    bb_ws_list_t *list = calloc(1, sizeof(*list));
    if (!list)
    {
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;
    return list;
}

int bb_ws_list_add(bb_ws_list_t *list, bb_websocket_t *ws)
{
    bb_ws_node_t *node = calloc(1, sizeof(*node));
    if (!node)
    {
        return -1;
    }
    node->ws = ws;
    node->prev = list->tail;
    if (list->head == NULL)
    {
        list->head = node;
    }
    else
    {
        list->tail->next = node;
    }
    list->tail = node;
    return 0;
}

int bb_ws_list_remove(bb_ws_list_t *list, bb_websocket_t *ws)
{
    for (bb_ws_node_t *node = list->head; node != NULL; node = node->next)
    {
        if (node->ws == ws)
        {
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
            return 0;
        }
    }
    return -1;
}

void bb_ws_list_destroy(bb_ws_list_t *list)
{
    bb_ws_node_t *node = list->head;
    while (node != NULL)
    {
        bb_websocket_destroy(node->ws);
        bb_ws_node_t *next = node->next;
        free(node);
        node = next;
    }
    free(list);
}
