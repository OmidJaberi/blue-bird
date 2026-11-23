#include "websocket/websocket_list.h"
#include <blue-bird/error/assert.h>
#include <stdbool.h>
#include <stdio.h>

/*
 * The list only stores the pointers, so opaque dummy addresses are enough.
 * Every entry is removed before bb_ws_list_destroy(), which would otherwise
 * call bb_websocket_destroy() on these fake sessions.
 */

static void test_count_null_list(void)
{
    printf("Testing ws_list count on NULL list...\n");
    BB_ASSERT(bb_ws_list_count(NULL) == 0);
}

static void test_count_add_remove(void)
{
    printf("Testing ws_list count add/remove...\n");
    bb_ws_list_t *list = bb_ws_list_create();
    BB_ASSERT(list != NULL);
    BB_ASSERT(bb_ws_list_count(list) == 0);

    int a, b, c;
    bb_websocket_t *wa = (bb_websocket_t *) &a;
    bb_websocket_t *wb = (bb_websocket_t *) &b;
    bb_websocket_t *wc = (bb_websocket_t *) &c;

    BB_ASSERT(bb_ws_list_add(list, wa) == 0);
    BB_ASSERT(bb_ws_list_add(list, wb) == 0);
    BB_ASSERT(bb_ws_list_add(list, wc) == 0);
    BB_ASSERT(bb_ws_list_count(list) == 3);

    BB_ASSERT(bb_ws_list_remove(list, wb) == 0); /* middle */
    BB_ASSERT(bb_ws_list_count(list) == 2);
    BB_ASSERT(bb_ws_list_remove(list, wa) == 0); /* head */
    BB_ASSERT(bb_ws_list_count(list) == 1);
    BB_ASSERT(bb_ws_list_remove(list, wc) == 0); /* tail */
    BB_ASSERT(bb_ws_list_count(list) == 0);

    bb_ws_list_destroy(list);
}

static void test_count_unknown_remove(void)
{
    printf("Testing ws_list count on unknown/double remove...\n");
    bb_ws_list_t *list = bb_ws_list_create();
    int a, b;
    bb_websocket_t *wa = (bb_websocket_t *) &a;
    bb_websocket_t *wb = (bb_websocket_t *) &b;

    BB_ASSERT(bb_ws_list_add(list, wa) == 0);

    /* Not in the list: must fail and leave the count alone. */
    BB_ASSERT(bb_ws_list_remove(list, wb) == -1);
    BB_ASSERT(bb_ws_list_count(list) == 1);

    BB_ASSERT(bb_ws_list_remove(list, wa) == 0);
    /* Second removal of the same session must not underflow the count. */
    BB_ASSERT(bb_ws_list_remove(list, wa) == -1);
    BB_ASSERT(bb_ws_list_count(list) == 0);

    bb_ws_list_destroy(list);
}

typedef struct
{
    bb_ws_list_t *list;
    bb_websocket_t *seen[8];
    int seen_count;
    bool remove_visited;
} ws_walk_t;

static void ws_record_visit(bb_websocket_t *ws, void *ctx)
{
    ws_walk_t *w = ctx;
    w->seen[w->seen_count++] = ws;
    if (w->remove_visited)
    {
        bb_ws_list_remove(w->list, ws);
    }
}

static void test_foreach_order_and_removal(void)
{
    printf("Testing ws_list foreach order and removal of the visited node...\n");

    int ids[4];
    ws_walk_t w = { .list = bb_ws_list_create() };
    BB_ASSERT(w.list != NULL);
    for (int i = 0; i < 4; i++)
    {
        BB_ASSERT(bb_ws_list_add(w.list, (bb_websocket_t *)&ids[i]) == 0);
    }

    bb_ws_list_foreach(w.list, ws_record_visit, &w);
    BB_ASSERT(w.seen_count == 4);
    for (int i = 0; i < 4; i++)
    {
        BB_ASSERT(w.seen[i] == (bb_websocket_t *)&ids[i]);
    }
    BB_ASSERT(bb_ws_list_count(w.list) == 4);

    /* Closing a session removes it from the list as it is being visited. */
    w.seen_count = 0;
    w.remove_visited = true;
    bb_ws_list_foreach(w.list, ws_record_visit, &w);
    BB_ASSERT(w.seen_count == 4);
    BB_ASSERT(bb_ws_list_count(w.list) == 0);

    /* Empty, NULL list, NULL visitor. */
    w.seen_count = 0;
    bb_ws_list_foreach(w.list, ws_record_visit, &w);
    bb_ws_list_foreach(NULL, ws_record_visit, &w);
    bb_ws_list_foreach(w.list, NULL, &w);
    BB_ASSERT(w.seen_count == 0);

    bb_ws_list_destroy(w.list);
}

int main(void)
{
    test_count_null_list();
    test_count_add_remove();
    test_count_unknown_remove();
    test_foreach_order_and_removal();
    printf("All ws_list tests passed.\n");
    return 0;
}
