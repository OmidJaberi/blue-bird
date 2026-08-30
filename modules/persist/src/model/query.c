#include "blue-bird/persist/query.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------
 * Builder
 * --------------------------- */

void bb_query_init(bb_query_t *q, bb_schema_t *schema)
{
    q->schema = schema;
    q->condition_count = 0;
    q->sort_count = 0;
    q->limit = -1;
    q->offset = -1;
}

int bb_query_where(bb_query_t *q, const char *field, bb_query_op_t op, const void *value)
{
    if (!q || !field || !value)
        return -1;

    if (!bb_schema_find_field(q->schema, field))
        return -1;

    if (q->condition_count >= BB_QUERY_MAX_CONDITIONS)
        return -1;

    q->conditions[q->condition_count++] = (bb_query_cond_t){
        .field = field,
        .op = op,
        .value = value
    };

    return 0;
}

int bb_query_order_by(bb_query_t *q, const char *field, bb_query_order_t direction)
{
    if (!q || !field)
        return -1;

    if (!bb_schema_find_field(q->schema, field))
        return -1;

    if (q->sort_count >= BB_QUERY_MAX_SORTS)
        return -1;

    q->sorts[q->sort_count++] = (bb_query_sort_t){
        .field = field,
        .direction = direction
    };

    return 0;
}

void bb_query_limit(bb_query_t *q, long limit)
{
    q->limit = limit;
}

void bb_query_offset(bb_query_t *q, long offset)
{
    q->offset = offset;
}

/* ---------------------------
 * Comparison
 *
 * Returns <0, 0, >0 the way memcmp/strcmp do. `field_ptr` points into an
 * entity struct at the field's offset; `value` is the raw typed value
 * pointer as passed to bb_query_where() (or a sort-owned copy for use in
 * bb_query_apply()'s comparator).
 * --------------------------- */

static int bb_field_compare(const bb_field_t *f, const void *field_ptr, const void *value)
{
    switch (f->type)
    {
        case BB_FIELD_INT:
        {
            int a = *(const int *)field_ptr;
            int b = *(const int *)value;
            return (a > b) - (a < b);
        }

        case BB_FIELD_STRING:
        case BB_FIELD_UUID:
            return strncmp((const char *)field_ptr, (const char *)value, f->size);

        case BB_FIELD_BLOB:
            return memcmp(field_ptr, value, f->size);

        default:
            return 0;
    }
}

/* Minimal '%'-wildcard match, same semantics as SQL LIKE for the subset
 * we support: '%' matches any run of characters (including none), every
 * other character must match literally. No '_' single-char wildcard and
 * no escaping -- enough for the common "starts with"/"contains"/"ends
 * with" cases without pulling in a full pattern engine. */
static int bb_like_match(const char *text, const char *pattern)
{
    if (*pattern == '\0')
        return *text == '\0';

    if (*pattern == '%')
    {
        /* collapse consecutive '%' */
        while (*pattern == '%')
            pattern++;

        if (*pattern == '\0')
            return 1; /* trailing '%' matches the rest of the string */

        for (const char *t = text; *t; t++)
        {
            if (bb_like_match(t, pattern))
                return 1;
        }

        return 0;
    }

    if (*text == '\0')
        return 0;

    if (*text != *pattern)
        return 0;

    return bb_like_match(text + 1, pattern + 1);
}

static int bb_cond_matches(const void *entity, const bb_query_t *q, const bb_query_cond_t *cond)
{
    bb_field_t *f = bb_schema_find_field(q->schema, cond->field);
    if (!f)
        return 0;

    const void *field_ptr = (const char *)entity + f->offset;

    if (cond->op == BB_OP_LIKE)
    {
        if (f->type != BB_FIELD_STRING && f->type != BB_FIELD_UUID)
            return 0;

        return bb_like_match((const char *)field_ptr, (const char *)cond->value);
    }

    int cmp = bb_field_compare(f, field_ptr, cond->value);

    switch (cond->op)
    {
        case BB_OP_EQ:  return cmp == 0;
        case BB_OP_NE:  return cmp != 0;
        case BB_OP_LT:  return cmp <  0;
        case BB_OP_LTE: return cmp <= 0;
        case BB_OP_GT:  return cmp >  0;
        case BB_OP_GTE: return cmp >= 0;
        default:        return 0;
    }
}

int bb_query_matches(const void *entity, const bb_query_t *q)
{
    for (size_t i = 0; i < q->condition_count; i++)
    {
        if (!bb_cond_matches(entity, q, &q->conditions[i]))
            return 0;
    }

    return 1;
}

/* ---------------------------
 * Sorting
 *
 * qsort()'s comparator takes no user context, so the query being sorted
 * against is stashed in a thread-local for the duration of the sort.
 * bb_query_apply() is the only caller and it's not reentrant across
 * threads sorting concurrently against different queries, which matches
 * the rest of this module (no internal locking anywhere yet).
 * --------------------------- */

static _Thread_local const bb_query_t *g_sort_query;
static _Thread_local size_t g_sort_struct_size;

static int bb_sort_cmp(const void *a, const void *b)
{
    const bb_query_t *q = g_sort_query;

    for (size_t i = 0; i < q->sort_count; i++)
    {
        const bb_query_sort_t *s = &q->sorts[i];
        bb_field_t *f = bb_schema_find_field(q->schema, s->field);
        if (!f)
            continue;

        const void *a_ptr = (const char *)a + f->offset;
        const void *b_ptr = (const char *)b + f->offset;

        int cmp = bb_field_compare(f, a_ptr, b_ptr);

        if (cmp != 0)
            return (s->direction == BB_ORDER_ASC) ? cmp : -cmp;
    }

    return 0;
}

int bb_query_apply(const bb_query_t *q, const void *in_array, size_t in_count, void **out_array, size_t *out_count)
{
    size_t struct_size = q->schema->struct_size;

    if (in_count == 0)
    {
        *out_array = NULL;
        *out_count = 0;
        return 0;
    }

    void *matched = malloc(struct_size * in_count);
    if (!matched)
        return -1;

    size_t matched_count = 0;

    for (size_t i = 0; i < in_count; i++)
    {
        const void *entity = (const char *)in_array + (i * struct_size);

        if (bb_query_matches(entity, q))
        {
            memcpy((char *)matched + (matched_count * struct_size), entity, struct_size);
            matched_count++;
        }
    }

    if (matched_count == 0)
    {
        free(matched);
        *out_array = NULL;
        *out_count = 0;
        return 0;
    }

    if (q->sort_count > 0)
    {
        g_sort_query = q;
        g_sort_struct_size = struct_size;
        qsort(matched, matched_count, struct_size, bb_sort_cmp);
        (void)g_sort_struct_size;
    }

    /* offset/limit */
    size_t start = (q->offset > 0) ? (size_t)q->offset : 0;

    if (start >= matched_count)
    {
        free(matched);
        *out_array = NULL;
        *out_count = 0;
        return 0;
    }

    size_t available = matched_count - start;
    size_t final_count = (q->limit >= 0 && (size_t)q->limit < available)
                              ? (size_t)q->limit
                              : available;

    if (start == 0 && final_count == matched_count)
    {
        *out_array = matched;
        *out_count = final_count;
        return 0;
    }

    void *page = malloc(struct_size * final_count);
    if (!page)
    {
        free(matched);
        return -1;
    }

    memcpy(page, (char *)matched + (start * struct_size), struct_size * final_count);
    free(matched);

    *out_array = page;
    *out_count = final_count;
    return 0;
}
