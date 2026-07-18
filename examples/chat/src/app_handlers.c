#include "app_handlers.h"
#include "app_repo.h"
#include "app_util.h"
#include "app_ws.h"

#include <blue-bird/log/log.h>
#include <blue-bird/utils/json.h>
#include <blue-bird/security/auth.h>
#include <blue-bird/security/password.h>
#include <blue-bird/security/session.h>
#include <blue-bird/utils/asset.h>

#include <stdlib.h>
#include <string.h>

bb_error_t serve_index(bb_request_t *req, bb_response_t *res)
{
    (void) req;

    bb_response_set_status(res, 200);
    bb_response_set_header(res, "Content-Type", "text/html; charset=utf-8");
    char *html;
    if (BB_FAILED(bb_asset_text_read_all("assets/index.html", &html, NULL)))
    {
        bb_response_set_status(res, 500);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to read asset");
    }
    bb_response_set_body(res, html);

    return BB_SUCCESS();
}

bb_error_t api_register(bb_request_t *req, bb_response_t *res)
{
    bb_json_t *body = app_parse_body_json(req);

    if (!body)
    {
        app_send_error(res, 400, "invalid JSON body");
        return BB_SUCCESS();
    }

    const char *username = app_json_get_string(body, "username");
    const char *password = app_json_get_string(body, "password");

    if (!app_valid_username(username))
    {
        bb_json_destroy(body);
        app_send_error(res, 400, "username must be 1-63 chars: letters, numbers, '_' or '-'");
        return BB_SUCCESS();
    }

    if (!password || strlen(password) < 4)
    {
        bb_json_destroy(body);
        app_send_error(res, 400, "password must be at least 4 characters");
        return BB_SUCCESS();
    }

    if (user_exists(&global_user_repo, username))
    {
        bb_json_destroy(body);
        app_send_error(res, 409, "username already taken");
        return BB_SUCCESS();
    }

    User u = {0};
    strncpy(u.username, username, sizeof(u.username) - 1);

    if (BB_FAILED(bb_password_hash(password, u.password_hash, sizeof(u.password_hash))))
    {
        bb_json_destroy(body);
        app_send_error(res, 500, "failed to hash password");
        return BB_SUCCESS();
    }

    bb_json_destroy(body);

    if (user_insert(&global_user_repo, &u) != 0)
    {
        app_send_error(res, 500, "failed to create user");
        return BB_SUCCESS();
    }

    bb_session_t session;

    if (BB_FAILED(bb_session_create(u.username, BB_SESSION_DEFAULT_TTL, &session)))
    {
        app_send_error(res, 500, "failed to create session");
        return BB_SUCCESS();
    }

    app_set_session_cookie(res, session.id);

    app_send_json(res, 201, OBJ(
        KEY("ok", BOOLV(true)),
        KEY("username", TEXTV(u.username))
    ));

    BB_LOG_INFO("[chat] registered user %s\n", u.username);

    return BB_SUCCESS();
}

static int verify_user_cb(const char *username, const char *password, char *user_id, size_t user_id_size)
{
    User u = {0};

    if (user_find_by_username(&global_user_repo, &u, username) != 0)
    {
        return 0;
    }

    if (!bb_password_verify(password, u.password_hash))
    {
        return 0;
    }

    strncpy(user_id, u.username, user_id_size - 1);
    user_id[user_id_size - 1] = '\0';

    return 1;
}

bb_error_t api_login(bb_request_t *req, bb_response_t *res)
{
    bb_json_t *body = app_parse_body_json(req);

    if (!body)
    {
        app_send_error(res, 400, "invalid JSON body");
        return BB_SUCCESS();
    }

    const char *username = app_json_get_string(body, "username");
    const char *password = app_json_get_string(body, "password");

    if (!username || !password)
    {
        bb_json_destroy(body);
        app_send_error(res, 400, "username and password are required");
        return BB_SUCCESS();
    }

    bb_session_t session;
    bb_error_t err = bb_auth_login(username, password, verify_user_cb, &session);

    if (BB_FAILED(err))
    {
        bb_json_destroy(body);
        app_send_error(res, 401, "invalid username or password");
        return BB_SUCCESS();
    }

    bb_json_destroy(body);

    app_set_session_cookie(res, session.id);

    app_send_json(res, 200, OBJ(
        KEY("ok", BOOLV(true)),
        KEY("username", TEXTV(session.user_id))
    ));

    return BB_SUCCESS();
}

bb_error_t api_logout(bb_request_t *req, bb_response_t *res)
{
    char session_id[BB_SESSION_ID_SIZE];

    if (app_get_cookie(req, "session_id", session_id, sizeof(session_id)) == 0)
    {
        bb_auth_logout(session_id);
    }

    app_clear_session_cookie(res);

    app_send_json(res, 200, OBJ(KEY("ok", BOOLV(true))));

    return BB_SUCCESS();
}

bb_error_t api_me(bb_request_t *req, bb_response_t *res)
{
    bb_session_t session;

    if (app_get_session(req, &session) != 0)
    {
        app_send_error(res, 401, "not authenticated");
        return BB_SUCCESS();
    }

    app_send_json(res, 200, OBJ(
        KEY("ok", BOOLV(true)),
        KEY("username", TEXTV(session.user_id))
    ));

    return BB_SUCCESS();
}

bb_error_t api_list_users(bb_request_t *req, bb_response_t *res)
{
    bb_session_t session;

    if (app_get_session(req, &session) != 0)
    {
        app_send_error(res, 401, "not authenticated");
        return BB_SUCCESS();
    }

    User *users = NULL;
    size_t count = 0;

    if (bb_repo_find_all(&global_user_repo.base, (void **) &users, &count) != 0)
    {
        app_send_error(res, 500, "failed to fetch users");
        return BB_SUCCESS();
    }

    bb_json_t *list = bb_json_new_array();

    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(users[i].username, session.user_id) == 0)
        {
            continue;
        }

        bb_json_array_push(list, OBJ(
            KEY("username", TEXTV(users[i].username)),
            KEY("online", BOOLV(chat_is_online(users[i].username)))
        ));
    }

    free(users);

    app_send_json(res, 200, OBJ(
        KEY("ok", BOOLV(true)),
        KEY("users", list)
    ));

    return BB_SUCCESS();
}

bb_error_t api_conversation(bb_request_t *req, bb_response_t *res)
{
    bb_session_t session;

    if (app_get_session(req, &session) != 0)
    {
        app_send_error(res, 401, "not authenticated");
        return BB_SUCCESS();
    }

    const char *peer = bb_request_get_param(req, "peer");

    if (!peer || !app_valid_username(peer))
    {
        app_send_error(res, 400, "invalid peer");
        return BB_SUCCESS();
    }

    Message *messages = NULL;
    size_t count = 0;

    if (message_find_conversation(&global_message_repo, session.user_id, peer, &messages, &count) != 0)
    {
        app_send_error(res, 500, "failed to fetch conversation");
        return BB_SUCCESS();
    }

    bb_json_t *list = bb_json_new_array();

    for (size_t i = 0; i < count; i++)
    {
        bb_json_array_push(list, OBJ(
            KEY("id", TEXTV(messages[i].id)),
            KEY("from", TEXTV(messages[i].from_user)),
            KEY("to", TEXTV(messages[i].to_user)),
            KEY("body", TEXTV(messages[i].body)),
            KEY("created_at", INTV(messages[i].created_at))
        ));
    }

    free(messages);

    app_send_json(res, 200, OBJ(
        KEY("ok", BOOLV(true)),
        KEY("peer", TEXTV(peer)),
        KEY("messages", list)
    ));

    return BB_SUCCESS();
}
