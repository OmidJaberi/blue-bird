#include "driver.h"

#include <stdlib.h>

void bb_migrate_applied_set_free(bb_migrate_applied_set_t *set)
{
    if (!set)
        return;

    free(set->items);
    set->items = NULL;
    set->count = 0;
}

void bb_migrate_driver_close(bb_migrate_driver_t *d)
{
    if (d && d->api && d->api->close)
        d->api->close(d);
}
