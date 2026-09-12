/*
 * Most of these tests exercise the PBKDF2 record format/parsing logic
 * directly through the backend, using a tiny iteration count (2) - that
 * logic doesn't depend on the iteration count, and a handful of these
 * checks running at the real production cost (BB_PASSWORD_PBKDF2_ITERATIONS,
 * currently 600,000) would make this file take tens of seconds to run.
 *
 * A small number of tests go through the real public API
 * (bb_password_hash()/bb_password_verify()) at full production cost, to
 * prove the end-to-end pipeline - and its buffer-size math - actually
 * works. Keep that list short.
 */

#include "blue-bird/security/password.h"

#include "password_backend.h"
#include "random.h"
#include "security_internal.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

/* Deliberately tiny - only the encoding/parsing logic is under test
 * here, not the cost parameter itself. */
#define TEST_ITERATIONS 2

static void backend_hash(const char *password, unsigned char *salt, char *out, size_t out_size)
{
    bb_error_t err = _bb_random_bytes(salt, BB_PASSWORD_SALT_SIZE);
    BB_ASSERT(err.code == BB_OK);

    err = _bb_password_backend_hash(
        password, salt, BB_PASSWORD_SALT_SIZE, TEST_ITERATIONS, out, out_size);
    BB_ASSERT(err.code == BB_OK);
}

void test_backend_format(void)
{
    printf("\tTesting backend record format...\n");
    fflush(stdout);

    unsigned char salt[BB_PASSWORD_SALT_SIZE];
    char record[256];

    backend_hash("secret123", salt, record, sizeof(record));

    BB_ASSERT(strncmp(record, "$pbkdf2-sha256$i=2$", 19) == 0);
}

void test_backend_verify_success_and_failure(void)
{
    printf("\tTesting backend verify success/failure...\n");
    fflush(stdout);

    unsigned char salt[BB_PASSWORD_SALT_SIZE];
    char record[256];

    backend_hash("secret123", salt, record, sizeof(record));

    BB_ASSERT(_bb_password_backend_verify("secret123", record) == 1);
    BB_ASSERT(_bb_password_backend_verify("wrong-password", record) == 0);
}

void test_backend_salt_uniqueness(void)
{
    printf("\tTesting backend salt uniqueness...\n");
    fflush(stdout);

    unsigned char salt1[BB_PASSWORD_SALT_SIZE];
    unsigned char salt2[BB_PASSWORD_SALT_SIZE];
    char record1[256];
    char record2[256];

    backend_hash("secret123", salt1, record1, sizeof(record1));
    backend_hash("secret123", salt2, record2, sizeof(record2));

    BB_ASSERT(strcmp(record1, record2) != 0);
}

void test_backend_reject_corrupted_hash(void)
{
    printf("\tTesting rejection of corrupted hash records...\n");
    fflush(stdout);

    unsigned char salt[BB_PASSWORD_SALT_SIZE];
    char record[256];

    backend_hash("secret123", salt, record, sizeof(record));

    size_t len = strlen(record);

    /* Flip the last character of the encoded hash. */
    record[len - 1] = (record[len - 1] == 'a') ? 'b' : 'a';

    BB_ASSERT(_bb_password_backend_verify("secret123", record) == 0);
}

void test_backend_reject_truncated_record(void)
{
    printf("\tTesting rejection of truncated hash records...\n");
    fflush(stdout);

    unsigned char salt[BB_PASSWORD_SALT_SIZE];
    char record[256];

    backend_hash("secret123", salt, record, sizeof(record));

    record[strlen(record) / 2] = '\0';

    BB_ASSERT(_bb_password_backend_verify("secret123", record) == 0);
}

void test_backend_reject_malformed_records(void)
{
    printf("\tTesting rejection of malformed records...\n");
    fflush(stdout);

    BB_ASSERT(_bb_password_backend_verify("secret123", "not-a-valid-record") == 0);
    BB_ASSERT(_bb_password_backend_verify("secret123", "$pbkdf2-sha256$i=0$aabb$ccdd") == 0);
    BB_ASSERT(_bb_password_backend_verify("secret123", "$pbkdf2-sha256$i=2$$ccdd") == 0);

    /* Invalid hex character in the salt, with an otherwise correctly
     * sized (64 hex char) hash field, so this actually exercises the
     * salt hex-decode failure path rather than just the length check. */
    BB_ASSERT(_bb_password_backend_verify(
        "secret123",
        "$pbkdf2-sha256$i=2$zz$"
        "0000000000000000000000000000000000000000000000000000000000000000") == 0);
}

void test_backend_empty_password(void)
{
    printf("\tTesting backend empty password round-trip...\n");
    fflush(stdout);

    unsigned char salt[BB_PASSWORD_SALT_SIZE];
    char record[256];

    backend_hash("", salt, record, sizeof(record));

    BB_ASSERT(_bb_password_backend_verify("", record) == 1);
    BB_ASSERT(_bb_password_backend_verify("not-empty", record) == 0);
}

void test_backend_buffer_too_small(void)
{
    printf("\tTesting backend hashing into an undersized buffer...\n");
    fflush(stdout);

    unsigned char salt[BB_PASSWORD_SALT_SIZE];

    bb_error_t err = _bb_random_bytes(salt, sizeof(salt));
    BB_ASSERT(err.code == BB_OK);

    char tiny[4];

    err = _bb_password_backend_hash("secret123", salt, sizeof(salt), TEST_ITERATIONS, tiny, sizeof(tiny));

    BB_ASSERT(err.code != BB_OK);
}

/* --- Public API: real production cost (BB_PASSWORD_PBKDF2_ITERATIONS).
 * Keep this list short - each of these takes real, deliberate time. --- */

void test_null_arguments(void)
{
    printf("\tTesting null arguments...\n");
    fflush(stdout);

    char hash[256];

    bb_error_t err = bb_password_hash(NULL, hash, sizeof(hash));

    BB_ASSERT(err.code == BB_ERR_NULL);
}

void test_password_hash_buffer_too_small(void)
{
    printf("\tTesting public API hashing into an undersized buffer...\n");
    fflush(stdout);

    /* Undersized enough to fail before any real PBKDF2 work happens. */
    char tiny[8];

    bb_error_t err = bb_password_hash("secret123", tiny, sizeof(tiny));

    BB_ASSERT(err.code != BB_OK);
}

void test_password_end_to_end_smoke(void)
{
    printf("\tTesting end-to-end password hash/verify at production cost (this one's slow by design)...\n");
    fflush(stdout);

    char hash[256];

    bb_error_t err = bb_password_hash("secret123", hash, sizeof(hash));

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(strncmp(hash, "bb$pbkdf2-sha256$i=", 19) == 0);

    BB_ASSERT(bb_password_verify("secret123", hash) == 1);
    BB_ASSERT(bb_password_verify("wrong-password", hash) == 0);
}

int main(void)
{
    printf("Running Password tests...\n");
    fflush(stdout);

    test_backend_format();
    test_backend_verify_success_and_failure();
    test_backend_salt_uniqueness();
    test_backend_reject_corrupted_hash();
    test_backend_reject_truncated_record();
    test_backend_reject_malformed_records();
    test_backend_empty_password();
    test_backend_buffer_too_small();

    test_null_arguments();
    test_password_hash_buffer_too_small();
    test_password_end_to_end_smoke();

    printf("All tests passed.\n");

    return 0;
}
