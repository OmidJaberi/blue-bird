#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "blue-bird/persist/key_val.h"
#include "blue-bird/persist/key_val/persist_json.h"

int main(void)
{
    const char *path = "test_json_data.json";

    /* Clean up */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);

    bb_persist_kv_json_register();
    bb_persist_kv_set_default("json");
    bb_persist_kv_set_default_uri(path);

    const char *msg = "json backend test successful";

    /* Save */
    if (bb_persist_kv_save("mykey", msg, strlen(msg)) != 0) {
        printf("FAIL: bb_persist_kv_save\n");
        return 1;
    }

    /* Load */
    char buf[128] = {0};
    if (bb_persist_kv_load("mykey", buf, sizeof(buf)) != 0) {
        printf("FAIL: bb_persist_kv_load\n");
        return 2;
    }

    if (strcmp(buf, msg) != 0) {
        printf("FAIL: File load mismatch\n");
        return 3;
    }

    /* Remove */
    if (bb_persist_kv_remove("mykey") != 0) {
        printf("FAIL: bb_persist_kv_remove\n");
        return 4;
    }

    /* Confirm deletion */
    if (bb_persist_kv_load("mykey", buf, sizeof(buf)) == 0) {
        printf("FAIL: key should not exist after deletion\n");
        return 5;
    }

    printf("File backend integration test passed!\n");
    return 0;
}
