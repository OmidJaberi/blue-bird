#if defined(_WIN32)
/* no POSIX feature-test macros needed on Windows */
#elif defined(__APPLE__)
#define _DARWIN_C_SOURCE
#define _XOPEN_SOURCE 700
#else
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#endif

#include "blue-bird/utils/dir.h"

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

#define TEST_DIR "test_dir_listing"

static void touch(const char *name)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s", TEST_DIR, name);

    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);
    fclose(f);
}

static void untouch(const char *name)
{
    char path[256];
    snprintf(path, sizeof path, "%s/%s", TEST_DIR, name);
    remove(path);
}

static void test_empty_directory(void)
{
    printf("\tTesting empty directory...\n");

    char **names = (char **)0x1;
    size_t count = 99;

    BB_ASSERT(bb_dir_list(TEST_DIR, &names, &count) == 0);
    BB_ASSERT(count == 0);
    BB_ASSERT(names == NULL);
    bb_dir_list_free(names, count);
}

static void test_sorted_and_no_dot_entries(void)
{
    printf("\tTesting names are sorted and exclude . and ..\n");

    /* Created out of order on purpose. */
    touch("b.sql");
    touch("0010_c.sql");
    touch("a.txt");
    touch("0002_b.sql");

    char **names = NULL;
    size_t count = 0;

    BB_ASSERT(bb_dir_list(TEST_DIR, &names, &count) == 0);
    BB_ASSERT(count == 4);
    BB_ASSERT(strcmp(names[0], "0002_b.sql") == 0);
    BB_ASSERT(strcmp(names[1], "0010_c.sql") == 0);
    BB_ASSERT(strcmp(names[2], "a.txt") == 0);
    BB_ASSERT(strcmp(names[3], "b.sql") == 0);
    bb_dir_list_free(names, count);

    /* A trailing separator makes no difference. */
    BB_ASSERT(bb_dir_list(TEST_DIR "/", &names, &count) == 0);
    BB_ASSERT(count == 4 && strcmp(names[0], "0002_b.sql") == 0);
    bb_dir_list_free(names, count);

    untouch("b.sql");
    untouch("0010_c.sql");
    untouch("a.txt");
    untouch("0002_b.sql");
}

static void test_errors(void)
{
    printf("\tTesting errors...\n");

    char **names = NULL;
    size_t count = 5;

    BB_ASSERT(bb_dir_list("test_dir_does_not_exist", &names, &count) == -1);
    BB_ASSERT(names == NULL && count == 0);

    BB_ASSERT(bb_dir_list(NULL, &names, &count) == -1);
    BB_ASSERT(bb_dir_list(TEST_DIR, NULL, &count) == -1);
    BB_ASSERT(bb_dir_list(TEST_DIR, &names, NULL) == -1);

    bb_dir_list_free(NULL, 0); /* NULL-safe */
}

int main(void)
{
    printf("Running directory listing unit tests...\n");

    /* Leftovers from an aborted earlier run would skew the counts. */
    REMOVE_DIR(TEST_DIR);
    BB_ASSERT(MAKE_DIR(TEST_DIR) == 0);

    test_empty_directory();
    test_sorted_and_no_dot_entries();
    test_errors();

    REMOVE_DIR(TEST_DIR);

    printf("All directory listing tests passed!\n");
    return 0;
}
