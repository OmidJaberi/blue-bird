#include "generate.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------
 * Small growable string
 * --------------------------- */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    int failed;
} strbuf_t;

static void sb_appendf(strbuf_t *sb, const char *fmt, ...)
{
    if (sb->failed)
        return;

    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);

    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (needed < 0)
    {
        va_end(ap2);
        sb->failed = 1;
        return;
    }

    if (sb->len + (size_t)needed + 1 > sb->cap)
    {
        size_t new_cap = sb->cap ? sb->cap : 256;

        while (new_cap < sb->len + (size_t)needed + 1)
            new_cap *= 2;

        char *grown = realloc(sb->data, new_cap);

        if (!grown)
        {
            va_end(ap2);
            sb->failed = 1;
            return;
        }

        sb->data = grown;
        sb->cap = new_cap;
    }

    vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, ap2);
    va_end(ap2);
    sb->len += (size_t)needed;
}

/* ---------------------------
 * Identifier safety
 *
 * Persist's backend writes table and column names into SQL unquoted, so
 * this does too (a generated migration should match). That is only safe
 * for plain identifiers; anything else is refused rather than escaped.
 * --------------------------- */

static int is_plain_identifier(const char *s)
{
    if (!s || !*s)
        return 0;

    if (!((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z') || *s == '_'))
        return 0;

    for (const char *p = s + 1; *p; p++)
    {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_'))
            return 0;
    }

    return 1;
}

/* ---------------------------
 * Collecting refusals
 * --------------------------- */

typedef struct {
    bb_migrate_err_t *err;
    const char *table;
    size_t count;
    size_t truncated;
} refusals_t;

static void refuse(refusals_t *r, const char *fmt, ...)
{
    char line[BB_MIGRATE_ERR_MAX];

    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);

    r->count++;

    if (!r->err)
        return;

    static const char bullet[] = "\n  - ";
    const size_t bullet_len = sizeof bullet - 1;

    size_t used = strlen(r->err->msg);
    size_t line_len = strlen(line);

    /* Leave room for the "(+N more)" and "write by hand" tails. */
    if (used + bullet_len + line_len + 1 + 96 > sizeof r->err->msg)
    {
        r->truncated++;
        return;
    }

    memcpy(r->err->msg + used, bullet, bullet_len);
    memcpy(r->err->msg + used + bullet_len, line, line_len + 1);
}

typedef struct {
    refusals_t *refusals;
} scan_ctx_t;

static void scan_for_refusals(const bb_migrate_diff_t *d, void *user)
{
    refusals_t *r = ((scan_ctx_t *)user)->refusals;
    char desc[BB_MIGRATE_ERR_MAX];

    bb_migrate_diff_describe(d, desc, sizeof desc);

    switch (d->kind)
    {
        case BB_MIGRATE_DIFF_TABLE_MISSING:
            break; /* supported */

        case BB_MIGRATE_DIFF_COLUMN_MISSING:
            if (d->expected_pk)
                refuse(r, "%s; SQLite can't add a primary key column with ALTER TABLE", desc);
            break;

        case BB_MIGRATE_DIFF_COLUMN_EXTRA:
            refuse(r, "%s; this could be a dropped or a renamed column and either can lose data", desc);
            break;

        case BB_MIGRATE_DIFF_TYPE_MISMATCH:
            refuse(r, "%s; changing a column's type needs a table rebuild", desc);
            break;

        case BB_MIGRATE_DIFF_PK_MISSING:
        case BB_MIGRATE_DIFF_PK_EXTRA:
            refuse(r, "%s; SQLite can't change a primary key with ALTER TABLE", desc);
            break;
    }
}

/* ---------------------------
 * Emitting SQL
 * --------------------------- */

static void emit_create_table(strbuf_t *sb, const bb_migrate_table_spec_t *spec)
{
    sb_appendf(sb, "CREATE TABLE IF NOT EXISTS %s (\n", spec->table);

    for (size_t i = 0; i < spec->count; i++)
    {
        const bb_migrate_column_t *c = &spec->columns[i];

        sb_appendf(sb, "    %s %s%s%s\n", c->name, c->type,
                   c->primary_key ? " PRIMARY KEY" : "",
                   i + 1 < spec->count ? "," : "");
    }

    sb_appendf(sb, ");\n");
}

typedef struct {
    strbuf_t *sb;
} emit_ctx_t;

static void emit_alter(const bb_migrate_diff_t *d, void *user)
{
    strbuf_t *sb = ((emit_ctx_t *)user)->sb;

    if (d->kind == BB_MIGRATE_DIFF_COLUMN_MISSING)
        sb_appendf(sb, "ALTER TABLE %s ADD COLUMN %s %s;\n", d->table, d->column, d->expected_type);
}

int bb_migrate_generate_sql(const bb_migrate_table_spec_t *expected,
                            const bb_migrate_table_spec_t *actual,
                            char **sql_out, bb_migrate_err_t *err)
{
    if (!expected || !actual || !sql_out)
    {
        bb_migrate_err_set(err, "invalid arguments");
        return BB_MIGRATE_GEN_REFUSED;
    }

    *sql_out = NULL;

    /* Names go into SQL unquoted, so they must be plain identifiers. */
    int names_ok = is_plain_identifier(expected->table);

    for (size_t i = 0; names_ok && i < expected->count; i++)
        names_ok = is_plain_identifier(expected->columns[i].name);

    if (!names_ok)
    {
        bb_migrate_err_set(err,
            "table '%s' can't be generated automatically:\n  - table and column names must be "
            "plain identifiers (letters, digits, underscore)", expected->table);
        return BB_MIGRATE_GEN_REFUSED;
    }

    /* Pass 1: is every difference one we are willing to generate? */
    if (err)
        snprintf(err->msg, sizeof err->msg, "table '%s' can't be generated automatically:", expected->table);

    refusals_t refusals = { err, expected->table, 0, 0 };
    scan_ctx_t scan = { &refusals };
    size_t diffs = bb_migrate_spec_diff(expected, actual, scan_for_refusals, &scan);

    if (refusals.count > 0)
    {
        if (err && refusals.truncated > 0)
        {
            size_t used = strlen(err->msg);
            snprintf(err->msg + used, sizeof err->msg - used, "\n  (+%zu more)", refusals.truncated);
        }

        if (err)
        {
            size_t used = strlen(err->msg);
            snprintf(err->msg + used, sizeof err->msg - used,
                     "\nwrite this migration by hand, then run `verify`.");
        }

        return BB_MIGRATE_GEN_REFUSED;
    }

    if (diffs == 0)
        return BB_MIGRATE_GEN_NOTHING;

    /* Pass 2: everything is supported, so emit it. */
    strbuf_t sb = { NULL, 0, 0, 0 };

    if (actual->count == 0)
    {
        emit_create_table(&sb, expected);
    }
    else
    {
        emit_ctx_t ctx = { &sb };
        bb_migrate_spec_diff(expected, actual, emit_alter, &ctx);
    }

    if (sb.failed || !sb.data)
    {
        free(sb.data);
        bb_migrate_err_set(err, "out of memory");
        return BB_MIGRATE_GEN_REFUSED;
    }

    *sql_out = sb.data;
    return BB_MIGRATE_GEN_OK;
}
