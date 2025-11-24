#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "persist/persist.h"
#include "persist/persist_file.h"

static void rmdir_recursive(const char *path)
{
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", path);
    system(cmd);
}

int main(void)
{
    const char *dir = "test_file_data/";

    /* Clean up */
    rmdir_recursive(dir);

    persist_file_register();
    persist_set_default("file");
    persist_set_default_uri(dir);   /* directory */

    const char *msg = "file backend test successful";

    /* Save */
    if (persist_save("mykey", msg, strlen(msg)) != 0) {
        printf("FAIL: persist_save\n");
        return 1;
    }

    /* Load */
    char buf[128] = {0};
    if (persist_load("mykey", buf, sizeof(buf)) != 0) {
        printf("FAIL: persist_load\n");
        return 2;
    }

    if (strcmp(buf, msg) != 0) {
        printf("FAIL: File load mismatch\n");
        return 3;
    }

    /* Remove */
    if (persist_remove("mykey") != 0) {
        printf("FAIL: persist_remove\n");
        return 4;
    }

    /* Confirm deletion */
    if (persist_load("mykey", buf, sizeof(buf)) == 0) {
        printf("FAIL: key should not exist after deletion\n");
        return 5;
    }

    printf("File backend integration test passed!\n");
    return 0;
}
