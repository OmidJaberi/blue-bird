#include <stdio.h>
#include <string.h>
#include "blue-bird/persist/key_val.h"

/* mock backend counters */
static int open_called=0, close_called=0, save_called=0;

static bb_persist_kv_handle_t* mock_open(const char *uri)
{
    (void) uri;
    open_called++;
    return (bb_persist_kv_handle_t*)0x1; // dummy pointer
}

static void mock_close(bb_persist_kv_handle_t *h)
{
    (void) h;
    close_called++;
}

static int mock_save(bb_persist_kv_handle_t *h, const char *key, const void *data, size_t size)
{
    (void) h;
    (void) key;
    (void) data;
    (void) size;
    save_called++;
    return 0;
}

/* static backend definition */
static bb_persist_kv_api_t mock_api = {
    .name = "mock",
    .open = mock_open,
    .close = mock_close,
    .save = mock_save,
    .load = NULL,
    .remove = NULL
};

int main(void)
{
    bb_persist_kv_register(&mock_api);
    bb_persist_kv_set_default("mock");
    bb_persist_kv_set_default_uri("whatever");

    bb_persist_kv_save("k", "v", 1);

    if (open_called != 1) return 1;
    if (save_called != 1) return 2;
    if (close_called != 1) return 3;

    printf("mock persist test passed!\n");
    return 0;
}
