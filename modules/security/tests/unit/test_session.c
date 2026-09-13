#include "blue-bird/security/session.h"
#include "blue-bird/security/config.h"

#include "session_store.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void test_session_create(void)
{
    printf("\tTesting session creation...\n");

    bb_session_t session;

    bb_error_t err = bb_session_create("user-1", 3600, &session);

    BB_ASSERT(err.code == BB_OK);

    BB_ASSERT(strlen(session.id) > 0);

    BB_ASSERT(strcmp(session.user_id, "user-1") == 0);

    BB_ASSERT(session.expires_at > time(NULL));
}

void test_session_lookup(void)
{
    printf("\tTesting session lookup...\n");

    bb_session_t created;
    bb_session_t fetched;

    bb_session_create("user-2", 3600, &created);

    bb_error_t err = bb_session_get(created.id, &fetched);

    BB_ASSERT(err.code == BB_OK);

    BB_ASSERT(strcmp(created.id, fetched.id) == 0);

    BB_ASSERT(strcmp(created.user_id, fetched.user_id) == 0);
}

void test_session_lookup_not_found(void)
{
    printf("\tTesting lookup of a session that was never created...\n");

    bb_session_t fetched;

    bb_error_t err = bb_session_get("00000000000000000000000000000000000000000000000000000000000000", &fetched);

    BB_ASSERT(err.code == BB_ERR_SESSION_NOT_FOUND);
}

void test_session_lookup_prefix_does_not_match(void)
{
    printf("\tTesting that a truncated/prefix ID does not match...\n");

    bb_session_t created;
    bb_session_t fetched;

    bb_session_create("user-prefix", 3600, &created);

    char prefix[BB_SESSION_ID_SIZE];

    strncpy(prefix, created.id, sizeof(prefix) - 1);
    prefix[sizeof(prefix) - 1] = '\0';
    prefix[strlen(prefix) / 2] = '\0'; /* truncate to half length */

    bb_error_t err = bb_session_get(prefix, &fetched);

    BB_ASSERT(err.code != BB_OK);
}

void test_session_lookup_empty_id(void)
{
    printf("\tTesting lookup with an empty ID string...\n");

    bb_session_t fetched;

    bb_error_t err = bb_session_get("", &fetched);

    BB_ASSERT(err.code != BB_OK);
}

void test_session_destroy(void)
{
    printf("\tTesting session destroy...\n");

    bb_session_t session;

    bb_session_create("user-3", 3600, &session);

    bb_error_t destroy_err = bb_session_destroy(session.id);

    BB_ASSERT(destroy_err.code == BB_OK);

    bb_error_t err = bb_session_get(session.id, &session);

    BB_ASSERT(err.code == BB_ERR_SESSION_NOT_FOUND);
}

void test_session_destroy_not_found(void)
{
    printf("\tTesting destroy of a session that doesn't exist...\n");

    bb_error_t err = bb_session_destroy("does-not-exist");

    BB_ASSERT(err.code == BB_ERR_SESSION_NOT_FOUND);
}

void test_session_expired(void)
{
    printf("\tTesting expired session...\n");

    bb_session_t session;

    bb_session_create("user-4", -1, &session);

    bb_error_t err = bb_session_get(session.id, &session);

    BB_ASSERT(err.code == BB_ERR_SESSION_EXPIRED);
}

void test_session_unique_ids(void)
{
    printf("\tTesting unique session ids...\n");

    bb_session_t s1;
    bb_session_t s2;

    bb_session_create("user-a", 3600, &s1);

    bb_session_create("user-b", 3600, &s2);

    BB_ASSERT(strcmp(s1.id, s2.id) != 0);
}

void test_session_null_arguments(void)
{
    printf("\tTesting null argument handling...\n");

    bb_session_t session;

    BB_ASSERT(bb_session_create(NULL, 3600, &session).code == BB_ERR_NULL);
    BB_ASSERT(bb_session_create("user-x", 3600, NULL).code == BB_ERR_NULL);

    BB_ASSERT(bb_session_get(NULL, &session).code == BB_ERR_NULL);
    BB_ASSERT(bb_session_get("some-id", NULL).code == BB_ERR_NULL);

    BB_ASSERT(bb_session_destroy(NULL).code == BB_ERR_NULL);
}

void test_session_cleanup_removes_only_expired(void)
{
    printf("\tTesting that cleanup removes expired sessions and keeps live ones...\n");

    bb_session_t expired;
    bb_session_t live;

    bb_session_create("user-expired", -5, &expired);
    bb_session_create("user-live", 3600, &live);

    size_t before = _bb_session_store_count();

    bb_error_t err = bb_session_cleanup_expired();

    BB_ASSERT(err.code == BB_OK);

    size_t after = _bb_session_store_count();

    /* At least the one expired session we just made should be gone;
     * other tests in this file may have left their own live sessions
     * lying around, so we can't assert an exact count. */
    BB_ASSERT(after < before);

    /* The live session must have survived the sweep. */
    bb_session_t fetched;
    BB_ASSERT(bb_session_get(live.id, &fetched).code == BB_OK);

    /* And the expired one must actually be gone from the store now,
     * not just rejected by the expiry check on lookup. */
    BB_ASSERT(bb_session_get(expired.id, &fetched).code == BB_ERR_SESSION_NOT_FOUND);
}

/* --- Config wiring --- */

void test_session_config_id_length_is_used(void)
{
    printf("\tTesting that a configured session ID length is used...\n");

    bb_security_config_t config;
    bb_security_config_default(&config);
    config.session_id_length = 32; /* the floor: 128 bits */

    bb_error_t set_err = bb_security_config_set(&config);
    BB_ASSERT(set_err.code == BB_OK);

    bb_session_t session;
    bb_error_t err = bb_session_create("user-short-id", 3600, &session);

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(strlen(session.id) == 32);

    /* It must still be independently look-up-able, not just the right
     * length. */
    bb_session_t fetched;
    BB_ASSERT(bb_session_get(session.id, &fetched).code == BB_OK);

    bb_security_config_default(&config);
    bb_security_config_set(&config);
}

void test_session_config_lifetime_default_used_by_login(void)
{
    printf("\tTesting that bb_auth_login()'s default lifetime tracks config...\n");

    /* auth.c's use of the configured session_lifetime_seconds is
     * exercised end-to-end in security.test_auth; here we just confirm
     * bb_session_create() honors whatever ttl it's given, which is the
     * piece auth.c relies on. */
    bb_session_t session;
    time_t before = time(NULL);

    bb_error_t err = bb_session_create("user-ttl", 120, &session);

    BB_ASSERT(err.code == BB_OK);
    BB_ASSERT(session.expires_at >= before + 120);
}

int main(void)
{
    printf("Running Session tests...\n");

    test_session_create();
    test_session_lookup();
    test_session_lookup_not_found();
    test_session_lookup_prefix_does_not_match();
    test_session_lookup_empty_id();
    test_session_destroy();
    test_session_destroy_not_found();
    test_session_expired();
    test_session_unique_ids();
    test_session_null_arguments();
    test_session_cleanup_removes_only_expired();
    test_session_config_id_length_is_used();
    test_session_config_lifetime_default_used_by_login();

    printf("All tests passed.\n");

    return 0;
}
