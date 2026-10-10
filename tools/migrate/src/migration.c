#include "migration.h"

#include <blue-bird/utils/hash.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------
 * Filename parsing
 * --------------------------- */

static const char *basename_of(const char *path)
{
    const char *base = path;

    for (const char *p = path; *p; p++)
    {
        if (*p == '/' || *p == '\\')
            base = p + 1;
    }

    return base;
}

static int is_name_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

int bb_migrate_parse_filename(const char *path, int64_t *version,
                              char *name, size_t name_cap,
                              bb_migrate_err_t *err)
{
    if (!path || !version || !name || name_cap == 0)
    {
        bb_migrate_err_set(err, "invalid arguments");
        return -1;
    }

    static const char ext[] = ".sql";
    const size_t ext_len = sizeof ext - 1;

    const char *base = basename_of(path);
    size_t len = strlen(base);

    if (len <= ext_len || strcmp(base + len - ext_len, ext) != 0)
    {
        bb_migrate_err_set(err,
            "'%s': migration files must be named <version>_<name>.sql", base);
        return -1;
    }

    size_t stem_len = len - ext_len;
    size_t i = 0;
    int64_t v = 0;

    while (i < stem_len && base[i] >= '0' && base[i] <= '9')
    {
        int digit = base[i] - '0';

        if (v > (INT64_MAX - digit) / 10)
        {
            bb_migrate_err_set(err, "'%s': version number is too large", base);
            return -1;
        }

        v = v * 10 + digit;
        i++;
    }

    if (i == 0 || i >= stem_len || base[i] != '_' || i + 1 >= stem_len)
    {
        bb_migrate_err_set(err,
            "'%s': migration files must be named <version>_<name>.sql", base);
        return -1;
    }

    i++; /* skip '_' */
    size_t name_len = stem_len - i;

    if (name_len >= name_cap)
    {
        bb_migrate_err_set(err, "'%s': migration name is too long (max %zu)",
                           base, name_cap - 1);
        return -1;
    }

    for (size_t k = 0; k < name_len; k++)
    {
        if (!is_name_char(base[i + k]))
        {
            bb_migrate_err_set(err,
                "'%s': migration name may only contain letters, digits, '_' and '-'",
                base);
            return -1;
        }
    }

    memcpy(name, base + i, name_len);
    name[name_len] = '\0';
    *version = v;
    return 0;
}

/* ---------------------------
 * Checksum
 * --------------------------- */

void bb_migrate_checksum(const char *data, size_t len,
                         char out[BB_MIGRATE_CHECKSUM_LEN + 1])
{
    static const char hex[] = "0123456789abcdef";

    bb_sha256_ctx ctx;
    bb_sha256_init(&ctx);

    /* Feed everything except the '\r' of each "\r\n" pair. */
    size_t start = 0;

    for (size_t i = 0; i + 1 < len; i++)
    {
        if (data[i] == '\r' && data[i + 1] == '\n')
        {
            if (i > start)
                bb_sha256_update(&ctx, data + start, i - start);

            start = i + 1;
        }
    }

    if (len > start)
        bb_sha256_update(&ctx, data + start, len - start);

    unsigned char digest[BB_SHA256_DIGEST_LENGTH];
    bb_sha256_final(&ctx, digest);

    for (size_t i = 0; i < BB_SHA256_DIGEST_LENGTH; i++)
    {
        out[i * 2]     = hex[digest[i] >> 4];
        out[i * 2 + 1] = hex[digest[i] & 0x0f];
    }

    out[BB_MIGRATE_CHECKSUM_LEN] = '\0';
}

/* ---------------------------
 * Loading
 * --------------------------- */

static int read_file(const char *path, char **out, size_t *out_len,
                     bb_migrate_err_t *err)
{
    FILE *f = fopen(path, "rb");

    if (!f)
    {
        bb_migrate_err_set(err, "cannot open '%s'", path);
        return -1;
    }

    size_t cap = 4096;
    size_t len = 0;
    char *buf = malloc(cap);

    if (!buf)
    {
        fclose(f);
        bb_migrate_err_set(err, "out of memory reading '%s'", path);
        return -1;
    }

    for (;;)
    {
        if (len + 1 >= cap)
        {
            char *grown = realloc(buf, cap * 2);

            if (!grown)
            {
                free(buf);
                fclose(f);
                bb_migrate_err_set(err, "out of memory reading '%s'", path);
                return -1;
            }

            buf = grown;
            cap *= 2;
        }

        size_t n = fread(buf + len, 1, cap - len - 1, f);

        if (n == 0)
            break;

        len += n;
    }

    int failed = ferror(f);
    fclose(f);

    if (failed)
    {
        free(buf);
        bb_migrate_err_set(err, "error reading '%s'", path);
        return -1;
    }

    buf[len] = '\0';
    *out = buf;
    *out_len = len;
    return 0;
}

static int compare_by_version(const void *a, const void *b)
{
    int64_t va = ((const bb_migration_t *)a)->version;
    int64_t vb = ((const bb_migration_t *)b)->version;

    return (va > vb) - (va < vb);
}

int bb_migrate_load_files(const char *const *paths, size_t count,
                          bb_migration_set_t *out, bb_migrate_err_t *err)
{
    if (!out)
    {
        bb_migrate_err_set(err, "invalid arguments");
        return -1;
    }

    out->items = NULL;
    out->count = 0;

    if (count == 0)
        return 0;

    if (!paths)
    {
        bb_migrate_err_set(err, "invalid arguments");
        return -1;
    }

    bb_migration_t *items = calloc(count, sizeof *items);

    if (!items)
    {
        bb_migrate_err_set(err, "out of memory");
        return -1;
    }

    for (size_t i = 0; i < count; i++)
    {
        bb_migration_t *m = &items[i];
        char *data = NULL;
        size_t len = 0;

        if (bb_migrate_parse_filename(paths[i], &m->version, m->name,
                                      sizeof m->name, err) != 0)
            goto fail;

        if (read_file(paths[i], &data, &len, err) != 0)
            goto fail;

        /* A NUL would silently truncate the SQL handed to the database. */
        if (memchr(data, '\0', len) != NULL)
        {
            free(data);
            bb_migrate_err_set(err, "'%s' contains a NUL byte", paths[i]);
            goto fail;
        }

        bb_migrate_checksum(data, len, m->checksum);
        m->sql = data;
    }

    qsort(items, count, sizeof *items, compare_by_version);

    for (size_t i = 1; i < count; i++)
    {
        if (items[i].version == items[i - 1].version)
        {
            bb_migrate_err_set(err, "duplicate migration version %lld ('%s' and '%s')",
                               (long long)items[i].version,
                               items[i - 1].name, items[i].name);
            goto fail;
        }
    }

    out->items = items;
    out->count = count;
    return 0;

fail:
    for (size_t i = 0; i < count; i++)
        free(items[i].sql);

    free(items);
    return -1;
}

void bb_migration_set_free(bb_migration_set_t *set)
{
    if (!set)
        return;

    for (size_t i = 0; i < set->count; i++)
        free(set->items[i].sql);

    free(set->items);
    set->items = NULL;
    set->count = 0;
}
