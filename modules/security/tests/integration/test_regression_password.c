/*
 * Adversarial password regression suite.
 *
 * This isn't testing that the happy path works (test_password.c does
 * that) - it's testing that the module behaves safely (no crash, no
 * hang, correct rejection) when handed the kinds of input a real
 * attacker or a corrupted database row would actually produce.
 *
 * Uses the backend directly at a low iteration count where the case
 * under test doesn't depend on cost (matching test_password.c's
 * approach - see that file for why).
 */

#include "blue-bird/security/config.h"
#include "blue-bird/security/password.h"

#include "password_backend.h"
#include "random.h"
#include "security_internal.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ITERATIONS 2

static void backend_hash(const char *password, char *out, size_t out_size)
{
    unsigned char salt[BB_PASSWORD_SALT_SIZE];

    bb_error_t err = _bb_random_bytes(salt, sizeof(salt));
    BB_ASSERT(err.code == BB_OK);

    err = _bb_password_backend_hash(password, salt, sizeof(salt), TEST_ITERATIONS, out, out_size);
    BB_ASSERT(err.code == BB_OK);
}

void test_adversarial_empty_password(void)
{
    printf("\tempty password...\n");
    fflush(stdout);

    char record[256];
    backend_hash("", record, sizeof(record));

    BB_ASSERT(_bb_password_backend_verify("", record) == 1);
    BB_ASSERT(_bb_password_backend_verify("not-empty", record) == 0);
}

void test_adversarial_very_long_password(void)
{
    printf("\tvery long password (64 KiB)...\n");
    fflush(stdout);

    size_t len = 64 * 1024;
    char *huge_password = malloc(len + 1);
    BB_ASSERT(huge_password != NULL);

    memset(huge_password, 'a', len);
    huge_password[len] = '\0';

    /* Must not crash or hang the backend regardless of policy - the
     * public API's own max-length policy is exercised separately in
     * test_password.c/test_password_config_max_length_is_enforced. */
    char record[256];
    backend_hash(huge_password, record, sizeof(record));

    BB_ASSERT(_bb_password_backend_verify(huge_password, record) == 1);

    /* Flipping one byte deep inside a huge password must still be
     * correctly detected as a different password. */
    huge_password[len / 2] ^= 0x01;
    BB_ASSERT(_bb_password_backend_verify(huge_password, record) == 0);

    free(huge_password);
}

void test_adversarial_public_api_rejects_overlong_password(void)
{
    printf("\tpublic API rejects a password over the configured maximum...\n");
    fflush(stdout);

    bb_security_config_t config;
    bb_security_config_default(&config);
    /* Keep this fast - the point is the length gate fires before any
     * hashing happens, not the cost of hashing itself. */
    config.password_pbkdf2_iterations = BB_PASSWORD_PBKDF2_ITERATIONS_MIN;

    bb_error_t set_err = bb_security_config_set(&config);
    BB_ASSERT(set_err.code == BB_OK);

    size_t len = config.password_max_length + 1;
    char *password = malloc(len + 1);
    BB_ASSERT(password != NULL);
    memset(password, 'x', len);
    password[len] = '\0';

    char hash[512];
    bb_error_t err = bb_password_hash(password, hash, sizeof(hash));

    BB_ASSERT(err.code == BB_ERR_PASSWORD_TOO_LONG);

    free(password);

    bb_security_config_default(&config);
    bb_security_config_set(&config);
}

void test_adversarial_wrong_password(void)
{
    printf("\twrong password...\n");
    fflush(stdout);

    char record[256];
    backend_hash("the-real-password", record, sizeof(record));

    BB_ASSERT(_bb_password_backend_verify("the-real-password", record) == 1);
    BB_ASSERT(_bb_password_backend_verify("The-Real-Password", record) == 0); /* case matters */
    BB_ASSERT(_bb_password_backend_verify("the-real-password ", record) == 0); /* trailing space matters */
    BB_ASSERT(_bb_password_backend_verify(" the-real-password", record) == 0); /* leading space matters */
    BB_ASSERT(_bb_password_backend_verify("the-real-passwor", record) == 0); /* one char short */
}

void test_adversarial_malformed_hash_shapes(void)
{
    printf("\tmalformed hash records (structurally broken)...\n");
    fflush(stdout);

    const char *malformed[] = {
        "",
        "bb$",
        "bb$pbkdf2-sha256$",
        "bb$pbkdf2-sha256$i=",
        "bb$pbkdf2-sha256$i=2",
        "bb$pbkdf2-sha256$i=2$",
        "bb$pbkdf2-sha256$i=2$aabb",
        "bb$pbkdf2-sha256$i=2$aabb$",
        "bb$pbkdf2-sha256$i=abc$aabb$ccdd", /* non-numeric iterations */
        "bb$pbkdf2-sha256$i=-5$aabb$ccdd",  /* negative iterations */
        "not even dollar-delimited at all",
        "$pbkdf2-sha256$i=2$aabb$ccdd", /* missing the "bb" tag entirely */
    };

    for (size_t i = 0; i < sizeof(malformed) / sizeof(malformed[0]); i++)
    {
        BB_ASSERT(bb_password_verify("whatever", malformed[i]) == 0);
    }
}

void test_adversarial_unsupported_algorithm_labels(void)
{
    printf("\tunsupported/legacy algorithm labels...\n");
    fflush(stdout);

    const char *unsupported[] = {
        "bb$sha256$deadbeef$deadbeef",      /* the framework's old, non-cryptographic format */
        "bb$fnv1a$deadbeef$deadbeef",       /* an even older hypothetical label */
        "bb$argon2id$v=19$m=65536,t=3,p=1$c2FsdA$aGFzaA", /* Phase 2's since-reverted approach */
        "bb$bcrypt$10$abcdefghijklmnopqrstuv",
        "bb$md5$deadbeef",
        "bb$plaintext$hunter2",
    };

    for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); i++)
    {
        BB_ASSERT(bb_password_verify("hunter2", unsupported[i]) == 0);
    }
}

void test_adversarial_corrupted_salt(void)
{
    printf("\tcorrupted salt (valid shape, wrong bytes)...\n");
    fflush(stdout);

    char record[256];
    backend_hash("secret123", record, sizeof(record));

    /* Locate the salt field: bb$pbkdf2-sha256$i=<n>$<salt>$<hash> */
    char *first_dollar_after_i = strchr(record, '=');
    BB_ASSERT(first_dollar_after_i != NULL);

    char *salt_start = strchr(first_dollar_after_i, '$');
    BB_ASSERT(salt_start != NULL);
    salt_start++; /* skip the '$' itself */

    /* Flip a hex character in the middle of the salt to a different,
     * still-valid hex character. This keeps the record's shape valid
     * (right lengths, right delimiters) so this exercises the "salt is
     * wrong" path specifically, not a parsing failure. */
    char original = salt_start[BB_PASSWORD_SALT_SIZE]; /* a char roughly mid-salt (hex, 2 chars/byte) */
    salt_start[BB_PASSWORD_SALT_SIZE] = (original == '0') ? '1' : '0';

    BB_ASSERT(_bb_password_backend_verify("secret123", record) == 0);
}

void test_adversarial_corrupted_hash(void)
{
    printf("\tcorrupted hash (valid shape, wrong bytes)...\n");
    fflush(stdout);

    char record[256];
    backend_hash("secret123", record, sizeof(record));

    size_t len = strlen(record);
    record[len - 1] = (record[len - 1] == '0') ? '1' : '0';

    BB_ASSERT(_bb_password_backend_verify("secret123", record) == 0);
}

int main(void)
{
    printf("Running adversarial password regression suite...\n");
    fflush(stdout);

    test_adversarial_empty_password();
    test_adversarial_very_long_password();
    test_adversarial_public_api_rejects_overlong_password();
    test_adversarial_wrong_password();
    test_adversarial_malformed_hash_shapes();
    test_adversarial_unsupported_algorithm_labels();
    test_adversarial_corrupted_salt();
    test_adversarial_corrupted_hash();

    printf("All tests passed.\n");

    return 0;
}
