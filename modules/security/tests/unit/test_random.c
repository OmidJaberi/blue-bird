#include "random.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

void test_random_bytes_length(void)
{
    printf("\tTesting random byte length...\n");

    unsigned char buffer[32];

    memset(buffer, 0, sizeof(buffer));

    bb_error_t err = _bb_random_bytes(buffer, sizeof(buffer));

    BB_ASSERT(err.code == BB_OK);
}

void test_random_bytes_non_determinism(void)
{
    printf("\tTesting random byte non-determinism...\n");

    unsigned char a[32];
    unsigned char b[32];

    _bb_random_bytes(a, sizeof(a));
    _bb_random_bytes(b, sizeof(b));

    BB_ASSERT(memcmp(a, b, sizeof(a)) != 0);
}

void test_random_bytes_zero_size(void)
{
    printf("\tTesting random byte request of size 0...\n");

    unsigned char buffer[4] = {0x11, 0x22, 0x33, 0x44};

    bb_error_t err = _bb_random_bytes(buffer, 0);

    BB_ASSERT(err.code == BB_OK);

    /* Nothing should have been touched. */
    BB_ASSERT(buffer[0] == 0x11);
    BB_ASSERT(buffer[1] == 0x22);
    BB_ASSERT(buffer[2] == 0x33);
    BB_ASSERT(buffer[3] == 0x44);
}

void test_random_bytes_null_buffer(void)
{
    printf("\tTesting random bytes with null buffer...\n");

    bb_error_t err = _bb_random_bytes(NULL, 16);

    BB_ASSERT(err.code == BB_ERR_NULL);
}

void test_random_hex_length_and_terminator(void)
{
    printf("\tTesting random hex length and terminator...\n");

    char buffer[17];

    bb_error_t err = _bb_random_hex(buffer, sizeof(buffer));

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(strlen(buffer) == 16);
    BB_ASSERT(buffer[16] == '\0');
}

void test_random_hex_odd_length(void)
{
    printf("\tTesting random hex with odd length...\n");

    char buffer[8];

    bb_error_t err = _bb_random_hex(buffer, sizeof(buffer));

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(strlen(buffer) == 7);
}

void test_random_hex_alphabet(void)
{
    printf("\tTesting random hex alphabet...\n");

    char buffer[65];

    _bb_random_hex(buffer, sizeof(buffer));

    for (size_t i = 0; i < strlen(buffer); i++)
    {
        char c = buffer[i];

        BB_ASSERT((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }
}

void test_random_hex_uniqueness(void)
{
    printf("\tTesting random hex uniqueness...\n");

    char a[33];
    char b[33];

    _bb_random_hex(a, sizeof(a));
    _bb_random_hex(b, sizeof(b));

    BB_ASSERT(strcmp(a, b) != 0);
}

void test_random_hex_null_buffer(void)
{
    printf("\tTesting random hex with null buffer...\n");

    bb_error_t err = _bb_random_hex(NULL, 16);

    BB_ASSERT(err.code == BB_ERR_NULL);
}

void test_random_hex_zero_size(void)
{
    printf("\tTesting random hex with zero size...\n");

    char buffer[1] = {'X'};

    bb_error_t err = _bb_random_hex(buffer, 0);

    BB_ASSERT(err.code != BB_OK);
}

int main(void)
{
    printf("Running Random tests...\n");

    test_random_bytes_length();
    test_random_bytes_non_determinism();
    test_random_bytes_zero_size();
    test_random_bytes_null_buffer();
    test_random_hex_length_and_terminator();
    test_random_hex_odd_length();
    test_random_hex_alphabet();
    test_random_hex_uniqueness();
    test_random_hex_null_buffer();
    test_random_hex_zero_size();

    printf("All tests passed.\n");

    return 0;
}
