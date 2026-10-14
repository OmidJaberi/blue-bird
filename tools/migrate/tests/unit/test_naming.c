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

static void test_empty_set(void)
{
    printf("\tTesting naming the first migration...\n");

    bb_migration_set_t set = { NULL, 0 };
    char file[128];
    bb_migrate_err_t err;

    BB_ASSERT(bb_migrate_next_version(&set) == 1);
    BB_ASSERT(bb_migrate_next_version(NULL) == 1);
    BB_ASSERT(bb_migrate_format_filename(&set, 1, "create_tasks", file, sizeof file, &err) == 0);
    BB_ASSERT(strcmp(file, "0001_create_tasks.sql") == 0);
}

static void test_counter_style(void)
{
    printf("\tTesting next version after counter-style migrations...\n");

    write_text_file("0001_a.sql", "SELECT 1;");
    write_text_file("0009_b.sql", "SELECT 2;");

    const char *paths[] = { "0009_b.sql", "0001_a.sql" };
    bb_migration_set_t set;
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_load_files(paths, 2, &set, &err) == 0);

    int64_t next = bb_migrate_next_version(&set);
    BB_ASSERT(next == 10);

    char file[128];
    BB_ASSERT(bb_migrate_format_filename(&set, next, "add_due", file, sizeof file, &err) == 0);
    BB_ASSERT(strcmp(file, "0010_add_due.sql") == 0);

    bb_migration_set_free(&set);
    remove("0001_a.sql");
    remove("0009_b.sql");
}

static void test_wide_versions_keep_their_width(void)
{
    printf("\tTesting timestamp-style numbering keeps its width...\n");

    write_text_file("20261009120000_init.sql", "SELECT 1;");

    const char *paths[] = { "20261009120000_init.sql" };
    bb_migration_set_t set;
    bb_migrate_err_t err;
    BB_ASSERT(bb_migrate_load_files(paths, 1, &set, &err) == 0);

    int64_t next = bb_migrate_next_version(&set);
    BB_ASSERT(next == 20261009120001LL);

    char file[128];
    BB_ASSERT(bb_migrate_format_filename(&set, next, "more", file, sizeof file, &err) == 0);
    BB_ASSERT(strcmp(file, "20261009120001_more.sql") == 0);

    bb_migration_set_free(&set);
    remove("20261009120000_init.sql");
}

static void test_bad_names_rejected(void)
{
    printf("\tTesting names that can't be loaded are rejected...\n");

    bb_migration_set_t set = { NULL, 0 };
    char file[128];
    bb_migrate_err_t err;

    const char *bad[] = {
        "", "has space", "dot.name", "slash/name", "../escape", "semi;colon",
        /* These end in a valid "<version>_<name>" component, so only a direct
         * character check catches them. */
        "x/0005_y", "..\\0005_y", "a\\b",
    };

    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        err.msg[0] = '\0';
        BB_ASSERT(bb_migrate_format_filename(&set, 1, bad[i], file, sizeof file, &err) != 0);
        BB_ASSERT(err.msg[0] != '\0');
    }

    char longname[300];
    memset(longname, 'a', sizeof longname - 1);
    longname[sizeof longname - 1] = '\0';
    BB_ASSERT(bb_migrate_format_filename(&set, 1, longname, file, sizeof file, &err) != 0);

    /* Output buffer too small. */
    char tiny[8];
    BB_ASSERT(bb_migrate_format_filename(&set, 1, "create_tasks", tiny, sizeof tiny, &err) != 0);
}

int main(void)
{
    printf("Running migration naming unit tests...\n");

    test_empty_set();
    test_counter_style();
    test_wide_versions_keep_their_width();
    test_bad_names_rejected();

    printf("All migration naming tests passed!\n");
    return 0;
}
