#ifndef BB_WEBSOCKET_LIST_H
#define BB_WEBSOCKET_LIST_H

#include "blue-bird/web/websocket/websocket.h"

#include <stddef.h>

typedef struct bb_ws_node {
    bb_websocket_t *ws;
    struct bb_ws_node *next;
    struct bb_ws_node *prev;
} bb_ws_node_t;

typedef struct {
    bb_ws_node_t *head;
    bb_ws_node_t *tail;
    size_t count;
} bb_ws_list_t;

bb_ws_list_t *bb_ws_list_create(void);
int bb_ws_list_add(bb_ws_list_t *list, bb_websocket_t *ws);
int bb_ws_list_remove(bb_ws_list_t *list, bb_websocket_t *ws);

/* Number of WebSocket sessions currently tracked (0 for a NULL list). O(1). */
size_t bb_ws_list_count(const bb_ws_list_t *list);

void bb_ws_list_destroy(bb_ws_list_t *list);

#endif //BB_WEBSOCKET_LIST_H
