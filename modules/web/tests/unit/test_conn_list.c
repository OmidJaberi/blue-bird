#include "connection/conn_list.h"
#include <blue-bird/error/assert.h>
#include <stdbool.h>
#include <stdio.h>

static void test_count_null_list(void)
{
    printf("Testing conn_list count on NULL list...\n");
    BB_ASSERT(bb_conn_list_count(NULL) == 0);
}

static void test_count_add_remove(void)
{
    printf("Testing conn_list count add/remove...\n");
    bb_conn_list_t *list = bb_conn_list_create();
    BB_ASSERT(list != NULL);
    BB_ASSERT(bb_conn_list_count(list) == 0);

    int a = 1, b = 2, c = 3;
    bb_conn_node_t *na = bb_conn_list_add(list, &a);
    bb_conn_node_t *nb = bb_conn_list_add(list, &b);
    bb_conn_node_t *nc = bb_conn_list_add(list, &c);
    BB_ASSERT(na && nb && nc);
    BB_ASSERT(bb_conn_list_count(list) == 3);

    bb_conn_list_remove(list, nb); /* middle */
    BB_ASSERT(bb_conn_list_count(list) == 2);
    bb_conn_list_remove(list, na); /* head */
    BB_ASSERT(bb_conn_list_count(list) == 1);
    bb_conn_list_remove(list, nc); /* tail */
    BB_ASSERT(bb_conn_list_count(list) == 0);

    bb_conn_list_destroy_all(list, NULL);
}

static void test_count_ignores_invalid_remove(void)
{
    printf("Testing conn_list count ignores NULL remove...\n");
    bb_conn_list_t *list = bb_conn_list_create();
    int a = 1;
    BB_ASSERT(bb_conn_list_add(list, &a) != NULL);

    bb_conn_list_remove(list, NULL);
    BB_ASSERT(bb_conn_list_count(list) == 1);

    bb_conn_list_destroy_all(list, NULL);
}

typedef struct
{
    bb_conn_list_t *list;
    bb_conn_node_t *nodes[8];
    int seen[8];
    int seen_count;
    bool remove_visited;
} walk_t;

static void record_visit(void *data, void *ctx)
{
    walk_t *w = ctx;
    int index = *(int *)data;
    w->seen[w->seen_count++] = index;
    if (w->remove_visited)
    {
        bb_conn_list_remove(w->list, w->nodes[index]);
    }
}

static void test_foreach_visits_oldest_first(void)
{
    printf("Testing conn_list foreach order...\n");
    walk_t w = { .list = bb_conn_list_create() };
    int ids[4] = { 0, 1, 2, 3 };
    for (int i = 0; i < 4; i++)
    {
        w.nodes[i] = bb_conn_list_add(w.list, &ids[i]);
        BB_ASSERT(w.nodes[i] != NULL);
    }

    bb_conn_list_foreach(w.list, record_visit, &w);

    BB_ASSERT(w.seen_count == 4);
    for (int i = 0; i < 4; i++)
    {
        BB_ASSERT(w.seen[i] == i);
    }
    BB_ASSERT(bb_conn_list_count(w.list) == 4);

    bb_conn_list_destroy_all(w.list, NULL);
}

/* The idle sweep closes -- and frees -- the connection it is visiting. */
static void test_foreach_allows_removing_the_visited_node(void)
{
    printf("Testing conn_list foreach while removing each visited node...\n");
    walk_t w = { .list = bb_conn_list_create(), .remove_visited = true };
    int ids[5] = { 0, 1, 2, 3, 4 };
    for (int i = 0; i < 5; i++)
    {
        w.nodes[i] = bb_conn_list_add(w.list, &ids[i]);
    }

    bb_conn_list_foreach(w.list, record_visit, &w);

    BB_ASSERT(w.seen_count == 5);
    BB_ASSERT(bb_conn_list_count(w.list) == 0);

    bb_conn_list_destroy_all(w.list, NULL);
}

static void test_foreach_edge_cases(void)
{
    printf("Testing conn_list foreach on empty/NULL...\n");
    walk_t w = { .list = bb_conn_list_create() };

    bb_conn_list_foreach(w.list, record_visit, &w); /* empty */
    bb_conn_list_foreach(NULL, record_visit, &w);   /* no list */
    int id = 0;
    w.nodes[0] = bb_conn_list_add(w.list, &id);
    bb_conn_list_foreach(w.list, NULL, &w);         /* no visitor */

    BB_ASSERT(w.seen_count == 0);

    bb_conn_list_destroy_all(w.list, NULL);
}

int main(void)
{
    test_count_null_list();
    test_count_add_remove();
    test_count_ignores_invalid_remove();
    test_foreach_visits_oldest_first();
    test_foreach_allows_removing_the_visited_node();
    test_foreach_edge_cases();
    printf("All conn_list tests passed.\n");

    return 0;
}
