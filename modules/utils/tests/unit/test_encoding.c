#include "blue-bird/utils/encoding.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_decode_percent(void)
{
    printf("\tTesting percent decoding...\n");

    char str[] = "hello%20world";

    bb_decode_percent(str, 0);

    assert(strcmp(str, "hello world") == 0);
}

void test_decode_plus(void)
{
    printf("\tTesting plus decoding...\n");

    char str[] = "hello+world";

    bb_decode_percent(str, 1);

    assert(strcmp(str, "hello world") == 0);
}

void test_base64_empty(void)
{
    printf("\tTesting base64 empty input...\n");

    char *encoded =
        bb_base64_encode("", 0);

    assert(encoded != NULL);

    assert(strcmp(encoded, "") == 0);

    free(encoded);
}

void test_base64_one_byte(void)
{
    printf("\tTesting base64 one byte...\n");

    char *encoded =
        bb_base64_encode("f", 1);

    assert(encoded != NULL);

    assert(strcmp(encoded, "Zg==") == 0);

    free(encoded);
}

void test_base64_two_bytes(void)
{
    printf("\tTesting base64 two bytes...\n");

    char *encoded =
        bb_base64_encode("fo", 2);

    assert(encoded != NULL);

    assert(strcmp(encoded, "Zm8=") == 0);

    free(encoded);
}

void test_base64_three_bytes(void)
{
    printf("\tTesting base64 three bytes...\n");

    char *encoded =
        bb_base64_encode("foo", 3);

    assert(encoded != NULL);

    assert(strcmp(encoded, "Zm9v") == 0);

    free(encoded);
}

void test_base64_rfc_examples(void)
{
    printf("\tTesting RFC base64 examples...\n");

    char *encoded;

    encoded = bb_base64_encode("foobar", 6);

    assert(encoded != NULL);

    assert(strcmp(encoded, "Zm9vYmFy") == 0);

    free(encoded);

    encoded = bb_base64_encode("hello", 5);

    assert(encoded != NULL);

    assert(strcmp(encoded, "aGVsbG8=") == 0);

    free(encoded);
}

int main(void)
{
    printf("Running Encoding tests...\n");

    test_decode_percent();

    test_decode_plus();

    test_base64_empty();

    test_base64_one_byte();

    test_base64_two_bytes();

    test_base64_three_bytes();

    test_base64_rfc_examples();

    printf("All tests passed.\n");

    return 0;
}