#include "blue-bird/utils/hash.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void digest_to_hex(const unsigned char digest[20], char hex[41])
{
    static const char chars[] = "0123456789abcdef";

    for (int i = 0; i < 20; ++i)
    {
        hex[i * 2] = chars[(digest[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = chars[digest[i] & 0x0F];
    }

    hex[40] = '\0';
}

void test_sha1_empty(void)
{
    printf("\tTesting SHA1 empty string...\n");

    unsigned char digest[20];
    char hex[41];

    bb_sha1("", 0, digest);

    digest_to_hex(digest, hex);

    assert(strcmp(hex, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0);
}

void test_sha1_abc(void)
{
    printf("\tTesting SHA1 abc...\n");

    unsigned char digest[20];
    char hex[41];

    bb_sha1("abc", 3, digest);

    digest_to_hex(digest, hex);

    assert(strcmp(hex, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
}

void test_sha1_long_vector(void)
{
    printf("\tTesting SHA1 long vector...\n");

    const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    unsigned char digest[20];
    char hex[41];

    bb_sha1(msg, strlen(msg), digest);

    digest_to_hex(digest, hex);

    assert(strcmp(hex, "84983e441c3bd26ebaae4aa1f95129e5e54670f1") == 0);
}

int main(void)
{
    printf("Running Hash tests...\n");

    test_sha1_empty();

    test_sha1_abc();

    test_sha1_long_vector();

    printf("All tests passed.\n");

    return 0;
}
