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
 * --rename parsing
 * --------------------------- */

int bb_migrate_parse_rename(const char *arg, bb_migrate_rename_t *out, bb_migrate_err_t *err)
{
    if (!arg || !out)
    {
        bb_migrate_err_set(err, "invalid arguments");
        return -1;
    }

    memset(out, 0, sizeof *out);

    const char *eq = strchr(arg, '=');
    const char *dot = strchr(arg, '.');

    /* Exactly one '.', and it must come before the '='. */
    if (!eq || !dot || dot > eq)
        goto bad;

    const char *second_dot = strchr(dot + 1, '.');

    if (second_dot && second_dot < eq)
        goto bad;

    size_t table_len = (size_t)(dot - arg);
    size_t from_len = (size_t)(eq - dot - 1);
    size_t to_len = strlen(eq + 1);

    if (table_len == 0 || from_len == 0 || to_len == 0 ||
        table_len >= sizeof out->table || from_len >= sizeof out->from || to_len >= sizeof out->to)
        goto bad;

    memcpy(out->table, arg, table_len);
    memcpy(out->from, dot + 1, from_len);
    memcpy(out->to, eq + 1, to_len);

    if (!is_plain_identifier(out->table) || !is_plain_identifier(out->from) ||
        !is_plain_identifier(out->to))
        goto bad;

    return 0;

bad:
    memset(out, 0, sizeof *out);
    bb_migrate_err_set(err,
        "--rename expects <table>.<old column>=<new column> (plain identifiers), got '%s'", arg);
    return -1;
}

/* ---------------------------
 * Per-table generation state
 * --------------------------- */

typedef struct {
    const bb_migrate_table_spec_t *expected;
    const bb_migrate_table_spec_t *actual;
    const bb_migrate_rename_t *renames;
    size_t rename_count;
    unsigned char mine[BB_MIGRATE_RENAMES_MAX];  /* hint is for this table      */
    unsigned char valid[BB_MIGRATE_RENAMES_MAX]; /* ...and is consistent        */
} gen_ctx_t;

static const bb_migrate_column_t *find_col(const bb_migrate_table_spec_t *spec, const char *name)
{
    for (size_t i = 0; i < spec->count; i++)
    {
        if (strcmp(spec->columns[i].name, name) == 0)
            return &spec->columns[i];
    }

    return NULL;
}

/* A valid rename accounts for BOTH of its diffs: `from` shows up as a
 * database-only column and `to` as a schema-only column. */
static int consumed_from(const gen_ctx_t *g, const char *column)
{
    for (size_t i = 0; i < g->rename_count; i++)
    {
        if (g->mine[i] && g->valid[i] && strcmp(g->renames[i].from, column) == 0)
            return 1;
    }

    return 0;
}

static int consumed_to(const gen_ctx_t *g, const char *column)
{
    for (size_t i = 0; i < g->rename_count; i++)
    {
        if (g->mine[i] && g->valid[i] && strcmp(g->renames[i].to, column) == 0)
            return 1;
    }

    return 0;
}

/* ---------------------------
 * Collecting refusals
 * --------------------------- */

typedef struct {
    bb_migrate_err_t *err;
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

static void validate_renames(gen_ctx_t *g, refusals_t *r)
{
    const bb_migrate_table_spec_t *exp = g->expected;
    const bb_migrate_table_spec_t *act = g->actual;

    for (size_t i = 0; i < g->rename_count; i++)
    {
        const bb_migrate_rename_t *rn = &g->renames[i];

        if (strcmp(rn->table, exp->table) != 0)
            continue;

        g->mine[i] = 1;

        if (!is_plain_identifier(rn->from) || !is_plain_identifier(rn->to))
        {
            refuse(r, "--rename %s.%s=%s: names must be plain identifiers", rn->table, rn->from, rn->to);
            continue;
        }

        const bb_migrate_column_t *a_from = find_col(act, rn->from);
        const bb_migrate_column_t *e_to = find_col(exp, rn->to);

        if (!a_from)
        {
            refuse(r, "--rename %s.%s=%s: the database has no column '%s'",
                   rn->table, rn->from, rn->to, rn->from);
            continue;
        }

        if (!e_to)
        {
            refuse(r, "--rename %s.%s=%s: the schema has no column '%s'",
                   rn->table, rn->from, rn->to, rn->to);
            continue;
        }

        if (find_col(exp, rn->from))
        {
            refuse(r, "--rename %s.%s=%s: the schema still has a column '%s', so it wasn't renamed",
                   rn->table, rn->from, rn->to, rn->from);
            continue;
        }

        if (find_col(act, rn->to))
        {
            refuse(r, "--rename %s.%s=%s: the database already has a column '%s'",
                   rn->table, rn->from, rn->to, rn->to);
            continue;
        }

        if (strcmp(a_from->type, e_to->type) != 0 || a_from->primary_key != e_to->primary_key)
        {
            refuse(r, "--rename %s.%s=%s: the type or primary key also changes (%s -> %s); "
                      "a rename plus a change is not generated, split it up or write it by hand",
                   rn->table, rn->from, rn->to, a_from->type, e_to->type);
            continue;
        }

        int conflict = 0;

        for (size_t j = 0; j < i; j++)
        {
            if (g->mine[j] && (strcmp(g->renames[j].from, rn->from) == 0 ||
                               strcmp(g->renames[j].to, rn->to) == 0))
                conflict = 1;
        }

        if (conflict)
        {
            refuse(r, "--rename %s.%s=%s: conflicts with another --rename for the same column",
                   rn->table, rn->from, rn->to);
            continue;
        }

        g->valid[i] = 1;
    }
}

/* ---------------------------
 * Scanning differences for refusals
 * --------------------------- */

typedef struct {
    gen_ctx_t *g;
    refusals_t *refusals;
} scan_ctx_t;

/* A database-only column plus a schema-only column of the same type is
 * probably a rename. Offer the specific hint only when it is unambiguous. */
static void refuse_extra_column(scan_ctx_t *c, const bb_migrate_diff_t *d, const char *desc)
{
    const bb_migrate_column_t *extra = find_col(c->g->actual, d->column);
    const bb_migrate_column_t *candidate = NULL;
    size_t candidates = 0;

    for (size_t i = 0; extra && i < c->g->expected->count; i++)
    {
        const bb_migrate_column_t *e = &c->g->expected->columns[i];

        if (!find_col(c->g->actual, e->name) && !consumed_to(c->g, e->name) &&
            strcmp(e->type, extra->type) == 0 && e->primary_key == extra->primary_key)
        {
            candidate = e;
            candidates++;
        }
    }

    if (candidates == 1)
    {
        refuse(c->refusals, "%s; if it was renamed to '%s', add --rename %s.%s=%s; "
                            "if it was dropped, that can lose data",
               desc, candidate->name, d->table, d->column, candidate->name);
    }
    else if (candidates > 1)
    {
        refuse(c->refusals, "%s; if it was renamed, add --rename %s.%s=<new name>; "
                            "if it was dropped, that can lose data",
               desc, d->table, d->column);
    }
    else
    {
        refuse(c->refusals, "%s; dropping a column can lose data", desc);
    }
}

static void scan_for_refusals(const bb_migrate_diff_t *d, void *user)
{
    scan_ctx_t *c = user;
    char desc[BB_MIGRATE_ERR_MAX];

    bb_migrate_diff_describe(d, desc, sizeof desc);

    switch (d->kind)
    {
        case BB_MIGRATE_DIFF_TABLE_MISSING:
            break; /* supported */

        case BB_MIGRATE_DIFF_COLUMN_MISSING:
            if (consumed_to(c->g, d->column))
                break; /* a rename creates it */

            if (d->expected_pk)
                refuse(c->refusals, "%s; SQLite can't add a primary key column with ALTER TABLE", desc);
            break;

        case BB_MIGRATE_DIFF_COLUMN_EXTRA:
            if (consumed_from(c->g, d->column))
                break; /* a rename accounts for it */

            refuse_extra_column(c, d, desc);
            break;

        case BB_MIGRATE_DIFF_TYPE_MISMATCH:
            refuse(c->refusals, "%s; changing a column's type needs a table rebuild", desc);
            break;

        case BB_MIGRATE_DIFF_PK_MISSING:
        case BB_MIGRATE_DIFF_PK_EXTRA:
            refuse(c->refusals, "%s; SQLite can't change a primary key with ALTER TABLE", desc);
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
    const gen_ctx_t *g;
} emit_ctx_t;

static void emit_alter(const bb_migrate_diff_t *d, void *user)
{
    emit_ctx_t *c = user;

    if (d->kind == BB_MIGRATE_DIFF_COLUMN_MISSING && !consumed_to(c->g, d->column))
        sb_appendf(c->sb, "ALTER TABLE %s ADD COLUMN %s %s;\n", d->table, d->column, d->expected_type);
}

int bb_migrate_generate_sql(const bb_migrate_table_spec_t *expected,
                            const bb_migrate_table_spec_t *actual,
                            const bb_migrate_rename_t *renames, size_t rename_count,
                            char **sql_out, bb_migrate_err_t *err)
{
    if (!expected || !actual || !sql_out || (rename_count > 0 && !renames) ||
        rename_count > BB_MIGRATE_RENAMES_MAX)
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

    gen_ctx_t g;
    memset(&g, 0, sizeof g);
    g.expected = expected;
    g.actual = actual;
    g.renames = renames;
    g.rename_count = rename_count;

    if (err)
        snprintf(err->msg, sizeof err->msg, "table '%s' can't be generated automatically:", expected->table);

    refusals_t refusals = { err, 0, 0 };

    /* Pass 0: do the rename hints for this table hold? */
    validate_renames(&g, &refusals);

    /* Pass 1: is every remaining difference one we are willing to generate? */
    scan_ctx_t scan = { &g, &refusals };
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
        /* Renames first, then additions. (Order doesn't change the outcome:
         * validation already refused any case where an added column would
         * reuse a renamed column's old name, since that is indistinguishable
         * from a type change. It just reads in the order a human would.) */
        for (size_t i = 0; i < rename_count; i++)
        {
            if (g.mine[i] && g.valid[i])
            {
                sb_appendf(&sb, "ALTER TABLE %s RENAME COLUMN %s TO %s;\n",
                           expected->table, renames[i].from, renames[i].to);
            }
        }

        emit_ctx_t ctx = { &sb, &g };
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
