#include "connection/conn_list.h"
#include <blue-bird/error/assert.h>
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

int main(void)
{
    test_count_null_list();
    test_count_add_remove();
    test_count_ignores_invalid_remove();
    printf("All conn_list tests passed.\n");
    return 0;
}
