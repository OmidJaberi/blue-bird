#include "table_spec.h"

#include <blue-bird/utils/json.h>

#include <stdio.h>
#include <string.h>

/* Keep in step with field_type_to_sql() in Persist's model_sqlite.c. */
static const char *sql_type_for(const char *manifest_type)
{
    if (strcmp(manifest_type, "int") == 0)
        return "INTEGER";

    if (strcmp(manifest_type, "string") == 0 || strcmp(manifest_type, "uuid") == 0)
        return "TEXT";

    if (strcmp(manifest_type, "blob") == 0)
        return "BLOB";

    return NULL;
}

static int copy_checked(char *dst, size_t cap, const char *src)
{
    size_t len = strlen(src);

    if (len == 0 || len >= cap)
        return -1;

    memcpy(dst, src, len + 1);
    return 0;
}

static const char *text_of(bb_json_t *obj, const char *key)
{
    bb_json_t *v = bb_json_object_get_value(obj, key);

    if (!v || bb_json_get_type(v) != BB_JSON_TEXT)
        return NULL;

    return bb_json_get_value_text(v);
}

int bb_migrate_spec_from_manifest(const char *path, bb_migrate_table_spec_t *out,
                                  bb_migrate_err_t *err)
{
    if (!path || !out)
    {
        bb_migrate_err_set(err, "invalid arguments");
        return -1;
    }

    memset(out, 0, sizeof *out);

    bb_json_t *manifest = bb_json_load(path);

    if (!manifest || bb_json_get_type(manifest) != BB_JSON_OBJECT)
    {
        if (manifest)
            bb_json_destroy(manifest);

        bb_migrate_err_set(err, "'%s': cannot read manifest (missing file or invalid JSON)", path);
        return -1;
    }

    int rc = -1;
    const char *kind = text_of(manifest, "kind");

    if (!kind || strcmp(kind, "persist.schema") != 0)
    {
        bb_migrate_err_set(err, "'%s': expected a manifest of kind \"persist.schema\"", path);
        goto done;
    }

    const char *table = text_of(manifest, "table");

    if (!table)
        table = text_of(manifest, "name");

    if (!table || copy_checked(out->table, sizeof out->table, table) != 0)
    {
        bb_migrate_err_set(err, "'%s': missing or invalid \"table\"/\"name\"", path);
        goto done;
    }

    bb_json_t *fields = bb_json_object_get_value(manifest, "fields");

    if (!fields || bb_json_get_type(fields) != BB_JSON_ARRAY || bb_json_get_size(fields) == 0)
    {
        bb_migrate_err_set(err, "'%s': \"fields\" must be a non-empty array", path);
        goto done;
    }

    size_t n = bb_json_get_size(fields);

    if (n > BB_MIGRATE_COLUMNS_MAX)
    {
        bb_migrate_err_set(err, "'%s': too many fields (max %d)", path, BB_MIGRATE_COLUMNS_MAX);
        goto done;
    }

    for (size_t i = 0; i < n; i++)
    {
        bb_json_t *f = bb_json_array_get_index(fields, (unsigned int)i);
        const char *name = f && bb_json_get_type(f) == BB_JSON_OBJECT ? text_of(f, "name") : NULL;
        const char *type = name ? text_of(f, "type") : NULL;
        const char *sql_type = type ? sql_type_for(type) : NULL;
        bb_migrate_column_t *c = &out->columns[i];

        if (!name || copy_checked(c->name, sizeof c->name, name) != 0)
        {
            bb_migrate_err_set(err, "'%s': field %zu has a missing or invalid \"name\"", path, i);
            goto done;
        }

        if (!sql_type)
        {
            bb_migrate_err_set(err, "'%s': field '%s' has a missing or unsupported \"type\"", path, name);
            goto done;
        }

        snprintf(c->type, sizeof c->type, "%s", sql_type);

        bb_json_t *pk = bb_json_object_get_value(f, "primary_key");
        c->primary_key = pk && bb_json_get_type(pk) == BB_JSON_BOOL && bb_json_get_value_bool(pk);
        out->count++;
    }

    rc = 0;

done:
    bb_json_destroy(manifest);

    if (rc != 0)
        memset(out, 0, sizeof *out);

    return rc;
}

static const bb_migrate_column_t *find_column(const bb_migrate_table_spec_t *spec, const char *name)
{
    for (size_t i = 0; i < spec->count; i++)
    {
        if (strcmp(spec->columns[i].name, name) == 0)
            return &spec->columns[i];
    }

    return NULL;
}

size_t bb_migrate_spec_diff(const bb_migrate_table_spec_t *expected,
                            const bb_migrate_table_spec_t *actual,
                            bb_migrate_diff_visit_cb on_diff, void *user)
{
    size_t diffs = 0;

#define EMIT(...) \
    do { \
        bb_migrate_diff_t d_ = { __VA_ARGS__ }; \
        diffs++; \
        if (on_diff) on_diff(&d_, user); \
    } while (0)

    if (actual->count == 0)
    {
        EMIT(BB_MIGRATE_DIFF_TABLE_MISSING, expected->table, NULL, NULL, NULL, 0);
        return diffs;
    }

    for (size_t i = 0; i < expected->count; i++)
    {
        const bb_migrate_column_t *e = &expected->columns[i];
        const bb_migrate_column_t *a = find_column(actual, e->name);

        if (!a)
        {
            EMIT(BB_MIGRATE_DIFF_COLUMN_MISSING, expected->table, e->name,
                 e->type, NULL, e->primary_key);
            continue;
        }

        if (strcmp(e->type, a->type) != 0)
        {
            EMIT(BB_MIGRATE_DIFF_TYPE_MISMATCH, expected->table, e->name,
                 e->type, a->type, 0);
        }

        if (e->primary_key && !a->primary_key)
            EMIT(BB_MIGRATE_DIFF_PK_MISSING, expected->table, e->name, NULL, NULL, 0);
        else if (!e->primary_key && a->primary_key)
            EMIT(BB_MIGRATE_DIFF_PK_EXTRA, expected->table, e->name, NULL, NULL, 0);
    }

    for (size_t i = 0; i < actual->count; i++)
    {
        if (!find_column(expected, actual->columns[i].name))
        {
            EMIT(BB_MIGRATE_DIFF_COLUMN_EXTRA, expected->table, actual->columns[i].name,
                 NULL, actual->columns[i].type, 0);
        }
    }

#undef EMIT

    return diffs;
}

void bb_migrate_diff_describe(const bb_migrate_diff_t *d, char *buf, size_t cap)
{
    switch (d->kind)
    {
        case BB_MIGRATE_DIFF_TABLE_MISSING:
            snprintf(buf, cap, "table '%s' does not exist after applying all migrations", d->table);
            break;
        case BB_MIGRATE_DIFF_COLUMN_MISSING:
            snprintf(buf, cap, "column '%s' (%s) is in the schema but no migration creates it",
                     d->column, d->expected_type);
            break;
        case BB_MIGRATE_DIFF_COLUMN_EXTRA:
            snprintf(buf, cap, "column '%s' is in the database but not in the schema", d->column);
            break;
        case BB_MIGRATE_DIFF_TYPE_MISMATCH:
            snprintf(buf, cap, "column '%s' is %s in the schema but %s in the database",
                     d->column, d->expected_type, d->actual_type);
            break;
        case BB_MIGRATE_DIFF_PK_MISSING:
            snprintf(buf, cap, "column '%s' is the primary key in the schema but not in the database",
                     d->column);
            break;
        case BB_MIGRATE_DIFF_PK_EXTRA:
            snprintf(buf, cap, "column '%s' is a primary key in the database but not in the schema",
                     d->column);
            break;
    }
}

typedef struct {
    bb_migrate_diff_cb on_message;
    void *user;
} describe_ctx_t;

static void describe_and_forward(const bb_migrate_diff_t *d, void *user)
{
    describe_ctx_t *ctx = user;
    char msg[BB_MIGRATE_ERR_MAX];

    bb_migrate_diff_describe(d, msg, sizeof msg);

    if (ctx->on_message)
        ctx->on_message(msg, ctx->user);
}

size_t bb_migrate_spec_compare(const bb_migrate_table_spec_t *expected,
                               const bb_migrate_table_spec_t *actual,
                               bb_migrate_diff_cb on_diff, void *user)
{
    describe_ctx_t ctx = { on_diff, user };
    return bb_migrate_spec_diff(expected, actual, describe_and_forward, &ctx);
}
