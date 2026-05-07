#include "utils/uuid.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_uuid_v4_generation(void)
{
    uint8_t uuid[16];

    int rc = bb_uuid_v4(uuid);
    assert(rc == 0);

    /* Version must be 4 */
    assert((uuid[6] & 0xF0) == 0x40);

    /* Variant must be 10xxxxxx */
    assert((uuid[8] & 0xC0) == 0x80);
}

static void test_uuid_string_format(void)
{
    char buf[BB_UUID_BUF_LEN];

    int rc = bb_uuid_v4_string(buf);
    assert(rc == 0);

    assert(strlen(buf) == 36);

    /* Check hyphen positions */
    assert(buf[8]  == '-');
    assert(buf[13] == '-');
    assert(buf[18] == '-');
    assert(buf[23] == '-');
}

static void test_uuid_uniqueness(void)
{
    char a[BB_UUID_BUF_LEN];
    char b[BB_UUID_BUF_LEN];

    bb_uuid_v4_string(a);
    bb_uuid_v4_string(b);

    /* Extremely unlikely to collide */
    assert(strcmp(a, b) != 0);
}

int main(void)
{
    test_uuid_v4_generation();
    test_uuid_string_format();
    test_uuid_uniqueness();

    printf("All UUID util tests passed.\n");
    return 0;
}