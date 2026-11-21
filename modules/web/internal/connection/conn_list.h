#ifndef BB_CONN_LIST_H
#define BB_CONN_LIST_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

typedef struct bb_conn_list bb_conn_list_t;
typedef struct bb_conn_node bb_conn_node_t;

typedef void (*bb_conn_cleanup_fn)(void *data);
typedef void (*bb_conn_visit_fn)(void *data, void *ctx);

bb_conn_list_t *bb_conn_list_create(void);

bb_conn_node_t *bb_conn_list_add(bb_conn_list_t *list, void *data);

void bb_conn_list_remove(bb_conn_list_t *list, bb_conn_node_t *node);

/* Number of connections currently tracked (0 for a NULL list). O(1). */
size_t bb_conn_list_count(const bb_conn_list_t *list);

void bb_conn_list_destroy_all(bb_conn_list_t *list, bb_conn_cleanup_fn cleanup);

/*
 * Calls visit(data, ctx) for every tracked connection, oldest first.
 *
 * visit may remove -- and free -- the connection it is called for (that is
 * how an expiry sweep closes it), but must not remove any other node or add
 * one while the walk is in progress.
 */
void bb_conn_list_foreach(bb_conn_list_t *list, bb_conn_visit_fn visit, void *ctx);


#ifdef __cplusplus
}
#endif

#endif //BB_CONN_LIST_H
