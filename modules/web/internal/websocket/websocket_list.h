#ifndef BB_WEBSOCKET_LIST_H
#define BB_WEBSOCKET_LIST_H

#include "blue-bird/web/websocket/websocket.h"

typedef struct bb_ws_node {
    bb_websocket_t *ws;
    struct bb_ws_node *next;
} bb_ws_node_t;

typedef struct {
    bb_ws_node_t *head;
    bb_ws_node_t *tail;
} bb_ws_list_t;

bb_ws_list_t *bb_ws_list_create(void);
int bb_ws_list_add(bb_ws_list_t *list, bb_websocket_t *ws);
void bb_ws_list_destroy(bb_ws_list_t *list);

#endif //BB_WEBSOCKET_LIST_H
