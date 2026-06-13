#include "blue-bird/security/session.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void test_session_create(void)
{
    printf("\tTesting session creation...\n");

    bb_session_t session;

    bb_error_t err = bb_session_create("user-1", 3600, &session);

    assert(err.code == BB_OK);

    assert(strlen(session.id) > 0);

    assert(strcmp(session.user_id, "user-1") == 0);

    assert(session.expires_at > time(NULL));
}

void test_session_lookup(void)
{
    printf("\tTesting session lookup...\n");

    bb_session_t created;
    bb_session_t fetched;

    bb_session_create("user-2", 3600, &created);

    bb_error_t err = bb_session_get(created.id, &fetched);

    assert(err.code == BB_OK);

    assert(strcmp(created.id, fetched.id) == 0);

    assert(strcmp(created.user_id, fetched.user_id) == 0);
}

void test_session_destroy(void)
{
    printf("\tTesting session destroy...\n");

    bb_session_t session;

    bb_session_create("user-3", 3600, &session);

    bb_session_destroy(session.id);

    bb_error_t err = bb_session_get(session.id, &session);

    assert(err.code != BB_OK);
}

void test_session_expired(void)
{
    printf("\tTesting expired session...\n");

    bb_session_t session;

    bb_session_create("user-4", -1, &session);

    bb_error_t err = bb_session_get(session.id, &session);

    assert(err.code != BB_OK);
}

void test_session_unique_ids(void)
{
    printf("\tTesting unique session ids...\n");

    bb_session_t s1;
    bb_session_t s2;

    bb_session_create("user-a", 3600, &s1);

    bb_session_create("user-b", 3600, &s2);

    assert(strcmp(s1.id, s2.id) != 0);
}

int main(void)
{
    printf("Running Session tests...\n");

    test_session_create();
    test_session_lookup();
    test_session_destroy();
    test_session_expired();
    test_session_unique_ids();

    printf("All tests passed.\n");

    return 0;
}
