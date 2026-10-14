#if defined(_WIN32)
/* no POSIX feature-test macros needed on Windows */
#elif defined(__APPLE__)
#define _DARWIN_C_SOURCE
#define _XOPEN_SOURCE 700
#else
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "migration.h"

#include <blue-bird/error/assert.h>

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define MAKE_DIR(p) _mkdir(p)
#define REMOVE_DIR(p) _rmdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MAKE_DIR(p) mkdir((p), 0755)
#define REMOVE_DIR(p) rmdir(p)
#endif

#define DIR_NAME "test_migrate_collect"

static void put(const char *name, const char *content)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s", DIR_NAME, name);

    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

static void drop(const char *name)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s", DIR_NAME, name);
    remove(path);
}

static void test_picks_only_sql_files(void)
{
    printf("\tTesting only .sql files are picked, sorted by name...\n");

    put("0010_c.sql", "SELECT 10;");
    put("0002_b.sql", "SELECT 2;");
    put("0001_a.sql", "SELECT 1;");
    put("README.md", "docs");
    put(".gitkeep", "");
    put("notes.txt", "x");

    char **paths = NULL;
    size_t count = 0;
    bb_migrate_err_t err;

    BB_ASSERT(bb_migrate_collect_paths(DIR_NAME, NULL, 0, 0, &paths, &count, &err) == 0);
    BB_ASSERT(count == 3);
    BB_ASSERT(strcmp(paths[0], DIR_NAME "/0001_a.sql") == 0);
    BB_ASSERT(strcmp(paths[1], DIR_NAME "/0002_b.sql") == 0);
    BB_ASSERT(strcmp(paths[2], DIR_NAME "/0010_c.sql") == 0);

    /* They load, and version order is numeric. */
    bb_migration_set_t set;
    BB_ASSERT(bb_migrate_load_files((const char *const *)paths, count, &set, &err) == 0);
    BB_ASSERT(set.count == 3 && set.items[2].version == 10);
    bb_migration_set_free(&set);
    bb_migrate_paths_free(paths, count);

    /* A trailing separator doesn't produce "//". */
    BB_ASSERT(bb_migrate_collect_paths(DIR_NAME "/", NULL, 0, 0, &paths, &count, &err) == 0);
    BB_ASSERT(count == 3 && strcmp(paths[0], DIR_NAME "/0001_a.sql") == 0);
    bb_migrate_paths_free(paths, count);

    drop("0010_c.sql");
    drop("0002_b.sql");
    drop("0001_a.sql");
    drop("README.md");
    drop(".gitkeep");
    drop("notes.txt");
}

static void test_dir_plus_explicit_files(void)
{
    printf("\tTesting explicit files are appended after the directory's...\n");

    put("0001_a.sql", "SELECT 1;");

    const char *files[] = { "somewhere/0009_extra.sql" };
    char **paths = NULL;
    size_t count = 0;
    bb_migrate_err_t err;

    BB_ASSERT(bb_migrate_collect_paths(DIR_NAME, files, 1, 0, &paths, &count, &err) == 0);
    BB_ASSERT(count == 2);
    BB_ASSERT(strcmp(paths[0], DIR_NAME "/0001_a.sql") == 0);
    BB_ASSERT(strcmp(paths[1], "somewhere/0009_extra.sql") == 0);
    BB_ASSERT(paths[1] != files[0]); /* copied: the caller owns the result */
    bb_migrate_paths_free(paths, count);

    /* No directory at all: just the files. */
    BB_ASSERT(bb_migrate_collect_paths(NULL, files, 1, 0, &paths, &count, &err) == 0);
    BB_ASSERT(count == 1 && strcmp(paths[0], files[0]) == 0);
    bb_migrate_paths_free(paths, count);

    drop("0001_a.sql");
}

static void test_mistyped_extension_is_an_error(void)
{
    printf("\tTesting a migration-looking file without .sql is an error...\n");

    put("0001_a.sql", "SELECT 1;");

    const char *bad[] = { "0003_add_index.SQL", "0004_oops.txt", "0005_noext" };

    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++)
    {
        put(bad[i], "SELECT 1;");

        char **paths = NULL;
        size_t count = 0;
        bb_migrate_err_t err;

        BB_ASSERT(bb_migrate_collect_paths(DIR_NAME, NULL, 0, 0, &paths, &count, &err) != 0);
        BB_ASSERT(strstr(err.msg, bad[i]) != NULL);
        BB_ASSERT(strstr(err.msg, "does not end in .sql") != NULL);
        BB_ASSERT(paths == NULL && count == 0);

        drop(bad[i]);
    }

    drop("0001_a.sql");
}

static void test_empty_and_missing_directory(void)
{
    printf("\tTesting empty, missing, and no-.sql directories...\n");

    char **paths = NULL;
    size_t count = 0;
    bb_migrate_err_t err;

    /* Empty directory and nothing else: refuse, don't pretend "nothing to do". */
    BB_ASSERT(bb_migrate_collect_paths(DIR_NAME, NULL, 0, 0, &paths, &count, &err) != 0);
    BB_ASSERT(strstr(err.msg, "no .sql migration files found") != NULL);

    put("README.md", "docs");
    BB_ASSERT(bb_migrate_collect_paths(DIR_NAME, NULL, 0, 0, &paths, &count, &err) != 0);
    BB_ASSERT(strstr(err.msg, "no .sql migration files found") != NULL);

    /* generate needs to start from an empty directory: allow_empty. */
    BB_ASSERT(bb_migrate_collect_paths(DIR_NAME, NULL, 0, 1, &paths, &count, &err) == 0);
    BB_ASSERT(count == 0);
    bb_migrate_paths_free(paths, count);

    /* ...but explicit files make that acceptable. */
    const char *files[] = { "0001_a.sql" };
    BB_ASSERT(bb_migrate_collect_paths(DIR_NAME, files, 1, 0, &paths, &count, &err) == 0);
    BB_ASSERT(count == 1);
    bb_migrate_paths_free(paths, count);
    drop("README.md");

    BB_ASSERT(bb_migrate_collect_paths("test_migrate_no_such_dir", NULL, 0, 0, &paths, &count, &err) != 0);
    BB_ASSERT(strstr(err.msg, "cannot read directory") != NULL);

    bb_migrate_paths_free(NULL, 0); /* NULL-safe */
}

int main(void)
{
    printf("Running migration path collection unit tests...\n");

    REMOVE_DIR(DIR_NAME);
    BB_ASSERT(MAKE_DIR(DIR_NAME) == 0);

    test_picks_only_sql_files();
    test_dir_plus_explicit_files();
    test_mistyped_extension_is_an_error();
    test_empty_and_missing_directory();

    REMOVE_DIR(DIR_NAME);

    printf("All migration path collection tests passed!\n");
    return 0;
}
