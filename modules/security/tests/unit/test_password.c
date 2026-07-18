#include "blue-bird/security/password.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

void test_password_hash(void)
{
    printf("\tTesting password hashing...\n");

    char hash[256];

    bb_error_t err = bb_password_hash("secret123", hash, sizeof(hash));

    BB_ASSERT(err.code == BB_OK);

    BB_ASSERT(strlen(hash) > 0);

    BB_ASSERT(strncmp(hash, "bb$", 3) == 0);
}

void test_password_verify_success(void)
{
    printf("\tTesting password verification success...\n");

    char hash[256];

    bb_password_hash("secret123", hash, sizeof(hash));

    BB_ASSERT(bb_password_verify("secret123", hash) == 1);
}

void test_password_verify_failure(void)
{
    printf("\tTesting password verification failure...\n");

    char hash[256];

    bb_password_hash("secret123", hash, sizeof(hash));

    BB_ASSERT(bb_password_verify("wrong-password", hash) == 0);
}

void test_password_hash_uniqueness(void)
{
    printf("\tTesting password salt uniqueness...\n");

    char hash1[256];
    char hash2[256];

    bb_password_hash("secret123", hash1, sizeof(hash1));

    bb_password_hash("secret123", hash2, sizeof(hash2));

    BB_ASSERT(strcmp(hash1, hash2) != 0);
}

void test_null_arguments(void)
{
    char hash[256];

    bb_error_t err = bb_password_hash(NULL, hash, sizeof(hash));

    BB_ASSERT(err.code == BB_ERR_NULL);
}

int main(void)
{
    printf("Running Password tests...\n");

    test_password_hash();
    test_password_verify_success();
    test_password_verify_failure();
    test_password_hash_uniqueness();
    test_null_arguments();

    printf("All tests passed.\n");

    return 0;
}
