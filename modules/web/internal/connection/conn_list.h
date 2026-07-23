#ifndef BB_CONN_LIST_H
#define BB_CONN_LIST_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>

typedef struct bb_conn_list bb_conn_list_t;
typedef struct bb_conn_node bb_conn_node_t;

typedef void (*bb_conn_cleanup_fn)(void *data);

bb_conn_list_t *bb_conn_list_create(void);

bb_conn_node_t *bb_conn_list_add(bb_conn_list_t *list, void *data);

void bb_conn_list_remove(bb_conn_list_t *list, bb_conn_node_t *node);

void bb_conn_list_destroy_all(bb_conn_list_t *list, bb_conn_cleanup_fn cleanup);


#ifdef __cplusplus
}
#endif

#endif //BB_CONN_LIST_H
