/*
 * Adversarial randomness regression suite.
 *
 * Zero-length and large requests are both real cases: zero-length
 * comes up naturally at PBKDF2/session-ID edges, and a large request
 * is the kind of thing that would expose a buffer-sizing bug that a
 * typical small test wouldn't.
 *
 * A note on what's *not* here: genuinely forcing the underlying OS
 * CSPRNG call itself to fail (getrandom()/BCryptGenRandom()/getentropy())
 * isn't something this suite fakes. Doing that convincingly and
 * portably across Linux/macOS/Windows would mean adding test-only
 * injection seams to random.c - code whose entire job is being a
 * minimal, auditable wrapper around the OS's own CSPRNG. That trade
 * (weakening/complicating the one file where correctness matters most,
 * to make a failure path easier to unit test) isn't worth it. What *is*
 * tested, here and in test_password.c/test_session.c, is that every
 * caller of _bb_random_bytes()/_bb_random_hex() checks the returned
 * bb_error_t and propagates failure rather than proceeding with a weak
 * or all-zero result (see also test_regression_invariants.c).
 */

#include "random.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_adversarial_zero_length_bytes(void)
{
    printf("\t_bb_random_bytes() with size 0...\n");
    fflush(stdout);

    unsigned char sentinel[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    unsigned char before[8];
    memcpy(before, sentinel, sizeof(sentinel));

    bb_error_t err = _bb_random_bytes(sentinel, 0);

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(memcmp(sentinel, before, sizeof(sentinel)) == 0); /* untouched */
}

void test_adversarial_zero_length_hex(void)
{
    printf("\t_bb_random_hex() with a 0-byte buffer (no room for NUL)...\n");
    fflush(stdout);

    char buffer[1] = {'X'};

    bb_error_t err = _bb_random_hex(buffer, 0);

    BB_ASSERT(err.code != BB_OK);
    BB_ASSERT(buffer[0] == 'X'); /* untouched - failure shouldn't write partial output */
}

void test_adversarial_large_request(void)
{
    printf("\t_bb_random_bytes() with a large (4 MiB) request...\n");
    fflush(stdout);

    size_t size = 4 * 1024 * 1024;
    unsigned char *buffer = malloc(size);
    BB_ASSERT(buffer != NULL);

    memset(buffer, 0, size);

    bb_error_t err = _bb_random_bytes(buffer, size);
    BB_ASSERT(err.code == BB_OK);

    /* A crude but effective "this isn't degenerate" check: a 4 MiB
     * buffer of real random bytes should not still be all zeros, and
     * should use a good spread of byte values. This is not a
     * statistical randomness test (that's out of scope for a unit
     * test) - it just catches the class of bug where a large request
     * silently falls back to leaving the buffer untouched. */
    int any_nonzero = 0;
    unsigned char seen[256] = {0};

    for (size_t i = 0; i < size; i++)
    {
        if (buffer[i] != 0)
            any_nonzero = 1;

        seen[buffer[i]] = 1;
    }

    BB_ASSERT(any_nonzero);

    int distinct_values = 0;
    for (int i = 0; i < 256; i++)
    {
        distinct_values += seen[i];
    }

    /* With 4 MiB of real random bytes we expect to see essentially all
     * 256 possible byte values at least once; require a generous but
     * meaningful floor. */
    BB_ASSERT(distinct_values > 200);

    free(buffer);
}

void test_adversarial_large_hex_request(void)
{
    printf("\t_bb_random_hex() with a large (1 MiB) request...\n");
    fflush(stdout);

    size_t hex_size = 1024 * 1024;
    char *buffer = malloc(hex_size);
    BB_ASSERT(buffer != NULL);

    bb_error_t err = _bb_random_hex(buffer, hex_size);
    BB_ASSERT(err.code == BB_OK);

    size_t len = strlen(buffer);
    BB_ASSERT(len == hex_size - 1);

    for (size_t i = 0; i < len; i++)
    {
        char c = buffer[i];
        BB_ASSERT((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'));
    }

    free(buffer);
}

void test_adversarial_repeated_calls_stay_independent(void)
{
    printf("\tmany consecutive calls never repeat...\n");
    fflush(stdout);

    enum { N = 1000 };
    unsigned char results[N][16];

    for (int i = 0; i < N; i++)
    {
        bb_error_t err = _bb_random_bytes(results[i], sizeof(results[i]));
        BB_ASSERT(err.code == BB_OK);
    }

    for (int i = 0; i < N; i++)
    {
        for (int j = i + 1; j < N; j++)
        {
            BB_ASSERT(memcmp(results[i], results[j], sizeof(results[i])) != 0);
        }
    }
}

int main(void)
{
    printf("Running adversarial randomness regression suite...\n");
    fflush(stdout);

    test_adversarial_zero_length_bytes();
    test_adversarial_zero_length_hex();
    test_adversarial_large_request();
    test_adversarial_large_hex_request();
    test_adversarial_repeated_calls_stay_independent();

    printf("All tests passed.\n");

    return 0;
}
