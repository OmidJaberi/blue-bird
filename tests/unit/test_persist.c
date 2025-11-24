#include <stdio.h>
#include <string.h>
#include "persist/persist.h"

/* mock backend counters */
static int open_called=0, close_called=0, save_called=0;

static PersistHandle* mock_open(const char *uri) {
    open_called++;
    return (PersistHandle*)0x1; // dummy pointer
}

static void mock_close(PersistHandle *h) {
    close_called++;
}

static int mock_save(PersistHandle *h, const char *key, const void *data, size_t size) {
    save_called++;
    return 0;
}

/* static backend definition */
static PersistAPI mock_api = {
    .name = "mock",
    .open = mock_open,
    .close = mock_close,
    .save = mock_save,
    .load = NULL,
    .remove = NULL
};

int main()
{
    persist_register(&mock_api);
    persist_set_default("mock");
    persist_set_default_uri("whatever");

    persist_save("k", "v", 1);

    if (open_called != 1) return 1;
    if (save_called != 1) return 2;
    if (close_called != 1) return 3;

    printf("mock persist test passed!\n");
    return 0;
}
