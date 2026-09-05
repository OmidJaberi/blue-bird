# Querying

`bb_query_t` is a small backend-agnostic criteria builder: `WHERE` conditions,
`ORDER BY`, `LIMIT`/`OFFSET`. It's the replacement for loading an entire
table and filtering client-side.

---

# Building a Query

```c
bb_query_t q;
bb_query_init(&q, &task_schema);

int min_age = 18;
bb_query_where(&q, "age", BB_OP_GTE, &min_age);
bb_query_where(&q, "name", BB_OP_LIKE, "A%");
bb_query_order_by(&q, "age", BB_ORDER_ASC);
bb_query_limit(&q, 20);
bb_query_offset(&q, 0);

Task *results = NULL;
size_t count = 0;
bb_repo_find(&repo, &q, (void **)&results, &count);

// ... use results ...
free(results);
```

Value pointers passed to `bb_query_where()` are **not copied** — they must
stay valid until `bb_repo_find()` runs, the same convention `insert()` /
`update()` use elsewhere in this module.

---

# Operators

```c
BB_OP_EQ    // =
BB_OP_NE    // !=
BB_OP_LT    // <
BB_OP_LTE   // <=
BB_OP_GT    // >
BB_OP_GTE   // >=
BB_OP_LIKE  // string fields only; '%' is a wildcard, as in SQL LIKE
```

---

# Native vs. Fallback Execution

`bb_repo_find()` is the only call site an application needs:

```c
int bb_repo_find(bb_repo_t *r, const bb_query_t *q, void **out_array, size_t *out_count);
```

- If the backend implements `bb_model_api_t.query`, the query is pushed
  down to the storage engine. The SQLite backend does this — `bb_query_t`
  becomes a real parameterized `SELECT ... WHERE ... ORDER BY ... LIMIT ?
  OFFSET ?`.
- If the backend leaves `query` `NULL` (the JSON backend, for example),
  `bb_repo_find()` falls back to `find_all()` followed by an in-memory
  filter/sort/paginate (`bb_query_apply()`).

Both paths are guaranteed to produce identical results for the same query —
some backends just do it more efficiently than others. This means new
backends get full query support for free the moment they implement basic
CRUD; native pushdown is an optimization they can add later, not a
prerequisite.

---

# Capacity

`bb_query_t` holds conditions and sort keys in fixed-size arrays
(`BB_QUERY_MAX_CONDITIONS` / `BB_QUERY_MAX_SORTS`, 8 and 4 by default) so it
stays stack-allocatable — no heap allocation just to build a query.
`bb_query_where()` / `bb_query_order_by()` return `-1` if the field doesn't
exist on the schema or the relevant array is full.
