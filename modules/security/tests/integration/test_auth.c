#include "blue-bird/security/auth.h"
#include "blue-bird/security/session.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

static int verify_user(const char *username, const char *password, char *user_id, size_t user_id_size)
{
    if (strcmp(username, "admin") == 0 && strcmp(password, "secret123") == 0)
    {
        strncpy(user_id, "user-1", user_id_size - 1);

        user_id[user_id_size - 1] = '\0';

        return 1;
    }

    return 0;
}

void test_login_success(void)
{
    printf("\tTesting successful login...\n");

    bb_session_t session;

    bb_error_t err = bb_auth_login("admin", "secret123", verify_user, &session);

    BB_ASSERT(err.code == BB_OK);

    BB_ASSERT(strlen(session.id) > 0);

    BB_ASSERT(strcmp(session.user_id, "user-1") == 0);
}

void test_login_failure(void)
{
    printf("\tTesting failed login...\n");

    bb_session_t session;

    bb_error_t err = bb_auth_login("admin", "wrong-password", verify_user, &session);

    BB_ASSERT(err.code != BB_OK);
}

void test_logout(void)
{
    printf("\tTesting logout...\n");

    bb_session_t session;

    bb_auth_login("admin", "secret123", verify_user, &session);

    bb_auth_logout(session.id);

    bb_error_t err = bb_session_get(session.id, &session);

    BB_ASSERT(err.code != BB_OK);
}

int main(void)
{
    printf("Running Auth tests...\n");

    test_login_success();
    test_login_failure();
    test_logout();

    printf("All tests passed.\n");

    return 0;
}
