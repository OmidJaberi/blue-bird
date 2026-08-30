#ifndef BB_PERSIST_QUERY_H
#define BB_PERSIST_QUERY_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/persist/schema.h"

/* ---------------------------
 * bb_query_t
 *
 * A small, backend-agnostic criteria/query builder: WHERE conditions,
 * ORDER BY, LIMIT/OFFSET. Backends that can push the query down to their
 * storage engine (e.g. SQLite -> a real SQL WHERE/ORDER BY/LIMIT) do so
 * via bb_model_api_t.query. Backends that can't (or don't yet) leave
 * that vtable slot NULL and bb_repo_find() falls back to evaluating the
 * same query in memory via bb_query_matches()/bb_query_apply(), so every
 * backend supports the same query semantics -- some just do it more
 * efficiently than others.
 *
 * Value pointers passed to bb_query_where() are NOT copied; they must
 * stay valid until the query is executed (same convention as insert()/
 * update() elsewhere in this module).
 * --------------------------- */

typedef enum {
    BB_OP_EQ,
    BB_OP_NE,
    BB_OP_LT,
    BB_OP_LTE,
    BB_OP_GT,
    BB_OP_GTE,
    BB_OP_LIKE   /* string fields only; '%' is a wildcard, as in SQL LIKE */
} bb_query_op_t;

typedef enum {
    BB_ORDER_ASC,
    BB_ORDER_DESC
} bb_query_order_t;

typedef struct {
    const char *field;
    bb_query_op_t op;
    const void *value;
} bb_query_cond_t;

typedef struct {
    const char *field;
    bb_query_order_t direction;
} bb_query_sort_t;

#define BB_QUERY_MAX_CONDITIONS 8
#define BB_QUERY_MAX_SORTS      4

typedef struct {
    bb_schema_t *schema;

    bb_query_cond_t conditions[BB_QUERY_MAX_CONDITIONS];
    size_t condition_count;

    bb_query_sort_t sorts[BB_QUERY_MAX_SORTS];
    size_t sort_count;

    long limit;   /* -1 = unset (no limit) */
    long offset;  /* -1 = unset (no offset) */
} bb_query_t;

/* Builder */

void bb_query_init(bb_query_t *q, bb_schema_t *schema);

/* Returns 0 on success, -1 if the field doesn't exist on the schema or
 * BB_QUERY_MAX_CONDITIONS has been reached. */
int bb_query_where(bb_query_t *q, const char *field, bb_query_op_t op, const void *value);

/* Returns 0 on success, -1 if the field doesn't exist on the schema or
 * BB_QUERY_MAX_SORTS has been reached. */
int bb_query_order_by(bb_query_t *q, const char *field, bb_query_order_t direction);

void bb_query_limit(bb_query_t *q, long limit);
void bb_query_offset(bb_query_t *q, long offset);

/* ---------------------------
 * In-memory evaluation
 *
 * Used directly by backends that store everything in memory anyway, and
 * used by bb_repo_find() as the fallback path for any backend that
 * leaves api->query NULL.
 * --------------------------- */

/* Does a single entity satisfy every condition in the query? */
int bb_query_matches(const void *entity, const bb_query_t *q);

/* Filters `in_array` (in_count entities of schema->struct_size each)
 * down to matching rows, applies ORDER BY, then LIMIT/OFFSET, and
 * allocates *out_array (caller frees) with the result. Returns 0 on
 * success (including a 0-row result), -1 on allocation failure. */
int bb_query_apply(const bb_query_t *q, const void *in_array, size_t in_count, void **out_array, size_t *out_count);


#ifdef __cplusplus
}
#endif

#endif //BB_PERSIST_QUERY_H
