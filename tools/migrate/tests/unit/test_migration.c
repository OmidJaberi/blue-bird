#include "migration.h"

#include <blue-bird/error/assert.h>

#include <stdio.h>
#include <string.h>

static void write_text_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

static void test_parse_filename_valid(void)
{
    printf("\tTesting filename parsing (valid)...\n");

    int64_t v = -1;
    char name[BB_MIGRATE_NAME_MAX];

    BB_ASSERT(bb_migrate_parse_filename("0002_add_email.sql", &v, name, sizeof name, NULL) == 0);
    BB_ASSERT(v == 2);
    BB_ASSERT(strcmp(name, "add_email") == 0);

    /* Only the final path component matters, with either separator. */
    BB_ASSERT(bb_migrate_parse_filename("db/migrations/10_x-y.sql", &v, name, sizeof name, NULL) == 0);
    BB_ASSERT(v == 10 && strcmp(name, "x-y") == 0);
    BB_ASSERT(bb_migrate_parse_filename("db\\m\\7_win.sql", &v, name, sizeof name, NULL) == 0);
    BB_ASSERT(v == 7 && strcmp(name, "win") == 0);

    /* Timestamp-style versions. */
    BB_ASSERT(bb_migrate_parse_filename("20261009120000_init.sql", &v, name, sizeof name, NULL) == 0);
    BB_ASSERT(v == 20261009120000LL);
}

static void test_parse_filename_invalid(void)
{
    printf("\tTesting filename parsing (invalid)...\n");

    int64_t v;
    char name[BB_MIGRATE_NAME_MAX];
    bb_migrate_err_t err;

    const char *bad[] = {
        "init.sql",                      /* no version */
        "0001.sql",                      /* no name */
        "0001_.sql",                     /* empty name */
        "_init.sql",                     /* empty version */
        "0001_init.txt",                 /* wrong extension */
        "0001_init",                     /* no extension */
        ".sql",
        "0001_has space.sql",            /* bad character */
        "0001_dot.name.sql",
        "99999999999999999999_big.sql",  /* overflows int64 */
        "",
    };

    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        err.msg[0] = '\0';
        BB_ASSERT(bb_migrate_parse_filename(bad[i], &v, name, sizeof name, &err) != 0);
        BB_ASSERT(err.msg[0] != '\0');
    }

    /* Name longer than the buffer. */
    char longname[300];
    memset(longname, 'a', sizeof longname);
    memcpy(longname, "1_", 2);
    memcpy(longname + 250, ".sql", 5);
    BB_ASSERT(bb_migrate_parse_filename(longname, &v, name, sizeof name, NULL) != 0);
}

static void test_checksum(void)
{
    printf("\tTesting checksum...\n");

    char a[BB_MIGRATE_CHECKSUM_LEN + 1];
    char b[BB_MIGRATE_CHECKSUM_LEN + 1];

    /* SHA-256("abc") */
    bb_migrate_checksum("abc", 3, a);
    BB_ASSERT(strcmp(a,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);

    /* CRLF and LF hash identically; a stray lone CR does not. */
    const char *lf = "CREATE TABLE t (id INTEGER);\nSELECT 1;\n";
    const char *crlf = "CREATE TABLE t (id INTEGER);\r\nSELECT 1;\r\n";
    bb_migrate_checksum(lf, strlen(lf), a);
    bb_migrate_checksum(crlf, strlen(crlf), b);
    BB_ASSERT(strcmp(a, b) == 0);

    const char *different = "CREATE TABLE t (id INTEGER);\nSELECT 2;\n";
    bb_migrate_checksum(different, strlen(different), b);
    BB_ASSERT(strcmp(a, b) != 0);

    /* Empty input is fine. */
    bb_migrate_checksum("", 0, a);
    BB_ASSERT(strlen(a) == BB_MIGRATE_CHECKSUM_LEN);
}

static void test_load_sorts_numerically(void)
{
    printf("\tTesting load sorts by numeric version...\n");

    write_text_file("0010_c.sql", "SELECT 10;");
    write_text_file("0002_b.sql", "SELECT 2;");
    write_text_file("0001_a.sql", "SELECT 1;");

    /* Deliberately lexically wrong order. */
    const char *paths[] = { "0010_c.sql", "0001_a.sql", "0002_b.sql" };

    bb_migration_set_t set;
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_load_files(paths, 3, &set, &err) == 0);
    BB_ASSERT(set.count == 3);
    BB_ASSERT(set.items[0].version == 1 && strcmp(set.items[0].name, "a") == 0);
    BB_ASSERT(set.items[1].version == 2);
    BB_ASSERT(set.items[2].version == 10);
    BB_ASSERT(strcmp(set.items[0].sql, "SELECT 1;") == 0);
    BB_ASSERT(strlen(set.items[0].checksum) == BB_MIGRATE_CHECKSUM_LEN);

    bb_migration_set_free(&set);
    BB_ASSERT(set.items == NULL && set.count == 0);

    remove("0010_c.sql");
    remove("0002_b.sql");
    remove("0001_a.sql");
}

static void test_load_rejects_duplicates(void)
{
    printf("\tTesting load rejects duplicate versions...\n");

    write_text_file("0001_one.sql", "SELECT 1;");
    write_text_file("1_uno.sql", "SELECT 1;");

    const char *paths[] = { "0001_one.sql", "1_uno.sql" };

    bb_migration_set_t set;
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_load_files(paths, 2, &set, &err) != 0);
    BB_ASSERT(strstr(err.msg, "duplicate") != NULL);
    BB_ASSERT(set.items == NULL && set.count == 0);

    remove("0001_one.sql");
    remove("1_uno.sql");
}

static void test_load_errors(void)
{
    printf("\tTesting load errors (missing file, bad name, empty list)...\n");

    bb_migration_set_t set;
    bb_migrate_err_t err;

    const char *missing[] = { "0001_does_not_exist.sql" };
    BB_ASSERT(bb_migrate_load_files(missing, 1, &set, &err) != 0);
    BB_ASSERT(strstr(err.msg, "cannot open") != NULL);

    write_text_file("notmigration.sql", "SELECT 1;");
    const char *badname[] = { "notmigration.sql" };
    BB_ASSERT(bb_migrate_load_files(badname, 1, &set, &err) != 0);
    remove("notmigration.sql");

    BB_ASSERT(bb_migrate_load_files(NULL, 0, &set, &err) == 0);
    BB_ASSERT(set.count == 0 && set.items == NULL);
}

int main(void)
{
    printf("Running migration source unit tests...\n");

    test_parse_filename_valid();
    test_parse_filename_invalid();
    test_checksum();
    test_load_sorts_numerically();
    test_load_rejects_duplicates();
    test_load_errors();

    printf("All migration source tests passed!\n");
    return 0;
}
