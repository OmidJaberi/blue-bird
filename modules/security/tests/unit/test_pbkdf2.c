#include "pbkdf2.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

static void to_hex(const unsigned char *data, size_t len, char *out)
{
    static const char digits[] = "0123456789abcdef";

    for (size_t i = 0; i < len; i++)
    {
        out[i * 2] = digits[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = digits[data[i] & 0x0F];
    }

    out[len * 2] = '\0';
}

void test_pbkdf2_one_iteration(void)
{
    printf("\tTesting PBKDF2-HMAC-SHA256 with 1 iteration...\n");

    unsigned char out[32];
    char hex[65];

    _bb_pbkdf2_hmac_sha256("password", 8, "salt", 4, 1, out, sizeof(out));
    to_hex(out, sizeof(out), hex);

    BB_ASSERT(strcmp(hex, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b") == 0);
}

void test_pbkdf2_two_iterations(void)
{
    printf("\tTesting PBKDF2-HMAC-SHA256 with 2 iterations...\n");

    unsigned char out[32];
    char hex[65];

    _bb_pbkdf2_hmac_sha256("password", 8, "salt", 4, 2, out, sizeof(out));
    to_hex(out, sizeof(out), hex);

    BB_ASSERT(strcmp(hex, "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43") == 0);
}

void test_pbkdf2_many_iterations(void)
{
    printf("\tTesting PBKDF2-HMAC-SHA256 with 4096 iterations...\n");

    unsigned char out[32];
    char hex[65];

    _bb_pbkdf2_hmac_sha256("password", 8, "salt", 4, 4096, out, sizeof(out));
    to_hex(out, sizeof(out), hex);

    BB_ASSERT(strcmp(hex, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a") == 0);
}

void test_pbkdf2_multi_block_output(void)
{
    printf("\tTesting PBKDF2-HMAC-SHA256 with output longer than one block...\n");

    const char *password = "passwordPASSWORDpassword";
    const char *salt = "saltSALTsaltSALTsaltSALTsaltSALTsalt";

    unsigned char out[40];
    char hex[81];

    _bb_pbkdf2_hmac_sha256(password, strlen(password), salt, strlen(salt), 4096, out, sizeof(out));
    to_hex(out, sizeof(out), hex);

    BB_ASSERT(strcmp(hex,
        "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e"
        "1c635518c7dac47e9") == 0);
}

void test_pbkdf2_deterministic(void)
{
    printf("\tTesting PBKDF2-HMAC-SHA256 determinism...\n");

    unsigned char a[32];
    unsigned char b[32];

    _bb_pbkdf2_hmac_sha256("same-password", 13, "same-salt", 9, 1000, a, sizeof(a));
    _bb_pbkdf2_hmac_sha256("same-password", 13, "same-salt", 9, 1000, b, sizeof(b));

    BB_ASSERT(memcmp(a, b, sizeof(a)) == 0);
}

void test_pbkdf2_different_salt_differs(void)
{
    printf("\tTesting PBKDF2-HMAC-SHA256 salt sensitivity...\n");

    unsigned char a[32];
    unsigned char b[32];

    _bb_pbkdf2_hmac_sha256("same-password", 13, "salt-one", 8, 1000, a, sizeof(a));
    _bb_pbkdf2_hmac_sha256("same-password", 13, "salt-two", 8, 1000, b, sizeof(b));

    BB_ASSERT(memcmp(a, b, sizeof(a)) != 0);
}

int main(void)
{
    printf("Running PBKDF2 tests...\n");

    test_pbkdf2_one_iteration();
    test_pbkdf2_two_iterations();
    test_pbkdf2_many_iterations();
    test_pbkdf2_multi_block_output();
    test_pbkdf2_deterministic();
    test_pbkdf2_different_salt_differs();

    printf("All tests passed.\n");

    return 0;
}
