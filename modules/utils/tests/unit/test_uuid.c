#include "blue-bird/utils/uuid.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

static void test_uuid_v4_generation(void)
{
    uint8_t uuid[16];

    int rc = bb_uuid_v4(uuid);
    BB_ASSERT(rc == 0);

    /* Version must be 4 */
    BB_ASSERT((uuid[6] & 0xF0) == 0x40);

    /* Variant must be 10xxxxxx */
    BB_ASSERT((uuid[8] & 0xC0) == 0x80);
}

static void test_uuid_string_format(void)
{
    bb_uuid_t buf;

    int rc = bb_uuid_v4_string(buf);
    BB_ASSERT(rc == 0);

    BB_ASSERT(strlen(buf) == 36);

    /* Check hyphen positions */
    BB_ASSERT(buf[8]  == '-');
    BB_ASSERT(buf[13] == '-');
    BB_ASSERT(buf[18] == '-');
    BB_ASSERT(buf[23] == '-');
}

static void test_uuid_uniqueness(void)
{
    bb_uuid_t a;
    bb_uuid_t b;

    bb_uuid_v4_string(a);
    bb_uuid_v4_string(b);

    /* Extremely unlikely to collide */
    BB_ASSERT(strcmp(a, b) != 0);
}

int main(void)
{
    test_uuid_v4_generation();
    test_uuid_string_format();
    test_uuid_uniqueness();

    printf("All UUID util tests passed.\n");
    return 0;
}
