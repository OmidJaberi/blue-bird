#include <stdio.h>
#include <string.h>
#include <unistd.h>     /* for unlink() */
#include "blue-bird/persist/key_val.h"
#include "blue-bird/persist/key_val/persist_sqlite.h"

/* Integration test for SQLite backend */
int main(void)
{
    const char *dbfile = "test_sqlite.db";

    /* Clean up from previous runs */
    unlink(dbfile);

    /* Register backend */
    bb_persist_kv_sqlite_register();
    bb_persist_kv_set_default("sqlite");
    bb_persist_kv_set_default_uri(dbfile);

    /* Save */
    const char *value = "hello world";
    if (bb_persist_kv_save("greeting", value, strlen(value)) != 0) {
        printf("FAIL: bb_persist_kv_save\n");
        return 1;
    }

    /* Load */
    char buf[64] = {0};
    if (bb_persist_kv_load("greeting", buf, sizeof(buf)) != 0) {
        printf("FAIL: bb_persist_kv_load\n");
        return 2;
    }

    if (strcmp(buf, value) != 0) {
        printf("FAIL: loaded value mismatch\n");
        return 3;
    }

    /* Remove */
    if (bb_persist_kv_remove("greeting") != 0) {
        printf("FAIL: bb_persist_kv_remove\n");
        return 4;
    }

    /* Confirm deletion */
    if (bb_persist_kv_load("greeting", buf, sizeof(buf)) == 0) {
        printf("FAIL: key should not exist after removal\n");
        return 5;
    }

    printf("SQLite integration test passed!\n");
    return 0;
}
