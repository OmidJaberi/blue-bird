#include "blue-bird/utils/hash.h"

#include <blue-bird/error/assert.h>
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

static void digest_to_hex_256(const unsigned char digest[32], char hex[65])
{
    static const char chars[] = "0123456789abcdef";

    for (int i = 0; i < 32; ++i)
    {
        hex[i * 2] = chars[(digest[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = chars[digest[i] & 0x0F];
    }

    hex[64] = '\0';
}

void test_sha1_empty(void)
{
    printf("\tTesting SHA1 empty string...\n");

    unsigned char digest[20];
    char hex[41];

    bb_sha1("", 0, digest);

    digest_to_hex(digest, hex);

    BB_ASSERT(strcmp(hex, "da39a3ee5e6b4b0d3255bfef95601890afd80709") == 0);
}

void test_sha1_abc(void)
{
    printf("\tTesting SHA1 abc...\n");

    unsigned char digest[20];
    char hex[41];

    bb_sha1("abc", 3, digest);

    digest_to_hex(digest, hex);

    BB_ASSERT(strcmp(hex, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0);
}

void test_sha1_long_vector(void)
{
    printf("\tTesting SHA1 long vector...\n");

    const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    unsigned char digest[20];
    char hex[41];

    bb_sha1(msg, strlen(msg), digest);

    digest_to_hex(digest, hex);

    BB_ASSERT(strcmp(hex, "84983e441c3bd26ebaae4aa1f95129e5e54670f1") == 0);
}

void test_sha256_empty(void)
{
    printf("\tTesting SHA256 empty string...\n");

    unsigned char digest[32];
    char hex[65];

    bb_sha256("", 0, digest);

    digest_to_hex_256(digest, hex);

    BB_ASSERT(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
}

void test_sha256_abc(void)
{
    printf("\tTesting SHA256 abc...\n");

    unsigned char digest[32];
    char hex[65];

    bb_sha256("abc", 3, digest);

    digest_to_hex_256(digest, hex);

    BB_ASSERT(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
}

void test_sha256_long_vector(void)
{
    printf("\tTesting SHA256 long vector...\n");

    const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    unsigned char digest[32];
    char hex[65];

    bb_sha256(msg, strlen(msg), digest);

    digest_to_hex_256(digest, hex);

    BB_ASSERT(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);
}

void test_hmac_sha256(void)
{
    printf("\tTesting HMAC-SHA256...\n");

    const char *msg = "The quick brown fox jumps over the lazy dog";

    unsigned char digest[32];
    char hex[65];

    bb_hmac_sha256("key", 3, msg, strlen(msg), digest);

    digest_to_hex_256(digest, hex);

    BB_ASSERT(strcmp(hex, "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8") == 0);
}

int main(void)
{
    printf("Running Hash tests...\n");

    test_sha1_empty();

    test_sha1_abc();

    test_sha1_long_vector();

    test_sha256_empty();

    test_sha256_abc();

    test_sha256_long_vector();

    test_hmac_sha256();

    printf("All tests passed.\n");

    return 0;
}
