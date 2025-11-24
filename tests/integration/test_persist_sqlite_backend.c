#include <stdio.h>
#include <string.h>
#include <unistd.h>     /* for unlink() */
#include "persist/persist.h"
#include "persist/persist_sqlite.h"

/* Integration test for SQLite backend */
int main(void)
{
    const char *dbfile = "test_sqlite.db";

    /* Clean up from previous runs */
    unlink(dbfile);

    /* Register backend */
    persist_sqlite_register();
    persist_set_default("sqlite");
    persist_set_default_uri(dbfile);

    /* Save */
    const char *value = "hello world";
    if (persist_save("greeting", value, strlen(value)) != 0) {
        printf("FAIL: persist_save\n");
        return 1;
    }

    /* Load */
    char buf[64] = {0};
    if (persist_load("greeting", buf, sizeof(buf)) != 0) {
        printf("FAIL: persist_load\n");
        return 2;
    }

    if (strcmp(buf, value) != 0) {
        printf("FAIL: loaded value mismatch\n");
        return 3;
    }

    /* Remove */
    if (persist_remove("greeting") != 0) {
        printf("FAIL: persist_remove\n");
        return 4;
    }

    /* Confirm deletion */
    if (persist_load("greeting", buf, sizeof(buf)) == 0) {
        printf("FAIL: key should not exist after removal\n");
        return 5;
    }

    printf("SQLite integration test passed!\n");
    return 0;
}
