#include "app_util.h"
#include "app_repo.h"

#include <blue-bird/utils/json.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

int app_get_cookie(bb_request_t *req, const char *name, char *out, size_t out_size)
{
    const char *header = bb_request_get_header(req, "Cookie");

    if (!header || out_size == 0)
    {
        return -1;
    }

    size_t name_len = strlen(name);
    const char *p = header;

    while (*p)
    {
        while (*p == ' ' || *p == '\t')
        {
            p++;
        }

        const char *eq = strchr(p, '=');
        if (!eq)
        {
            break;
        }

        const char *semi = strchr(p, ';');
        size_t key_len = (size_t)(eq - p);

        if (key_len == name_len && strncmp(p, name, name_len) == 0)
        {
            const char *val_start = eq + 1;
            size_t val_len = semi ? (size_t)(semi - val_start) : strlen(val_start);

            if (val_len >= out_size)
            {
                val_len = out_size - 1;
            }

            memcpy(out, val_start, val_len);
            out[val_len] = '\0';
            return 0;
        }

        if (!semi)
        {
            break;
        }

        p = semi + 1;
    }

    return -1;
}

void app_set_session_cookie(bb_response_t *res, const char *session_id)
{
    /*
     * Intentionally NOT HttpOnly: the frontend needs to read this cookie
     * client-side so it can hand the session id to the websocket during
     * its own "auth" handshake (the server's public websocket API has no
     * connect hook that exposes the original HTTP request/cookies to the
     * app, so the client must present the session id itself as the first
     * frame). For a production app you'd want a short-lived, purpose-built
     * websocket ticket instead of reusing the HTTP session cookie this way.
     */
    char cookie[256];
    snprintf(cookie, sizeof(cookie), "session_id=%s; Path=/; SameSite=Lax; Max-Age=%d", session_id, BB_SESSION_DEFAULT_TTL);
    bb_response_set_header(res, "Set-Cookie", cookie);
}

void app_clear_session_cookie(bb_response_t *res)
{
    bb_response_set_header(res, "Set-Cookie", "session_id=; Path=/; SameSite=Lax; Max-Age=0");
}

int app_get_session(bb_request_t *req, bb_session_t *session)
{
    char session_id[BB_SESSION_ID_SIZE];

    if (app_get_cookie(req, "session_id", session_id, sizeof(session_id)) != 0)
    {
        return -1;
    }

    if (BB_FAILED(bb_session_get(session_id, session)))
    {
        return -1;
    }

    return 0;
}

bb_json_t *app_parse_body_json(bb_request_t *req)
{
    const char *body = bb_request_get_body(req);

    if (!body || body[0] == '\0')
    {
        return NULL;
    }

    char *copy = strdup(body);
    if (!copy)
    {
        return NULL;
    }

    bb_json_t *json = bb_json_parse(copy);

    if (!json)
    {
        free(copy);
        return NULL;
    }

    free(copy);

    if (bb_json_get_type(json) != BB_JSON_OBJECT)
    {
        bb_json_destroy(json);
        return NULL;
    }

    return json;
}

const char *app_json_get_string(bb_json_t *obj, const char *key)
{
    if (!obj)
    {
        return NULL;
    }

    bb_json_t *node = bb_json_object_get_value(obj, key);

    if (!node || bb_json_get_type(node) != BB_JSON_TEXT)
    {
        return NULL;
    }

    return bb_json_get_value_text(node);
}

void app_send_json(bb_response_t *res, int status, bb_json_t *json)
{
    char *buf = NULL;
    int size = 0;

    bb_json_serialize(json, &buf, &size);
    bb_json_destroy(json);

    bb_response_set_status(res, status);
    bb_response_set_header(res, "Content-Type", "application/json");

    if (buf)
    {
        bb_response_set_body(res, buf);
        free(buf);
    }
    else
    {
        bb_response_set_body(res, "{}");
    }
}

void app_send_error(bb_response_t *res, int status, const char *msg)
{
    bb_json_t *json = OBJ(
        KEY("ok", BOOL(false)),
        KEY("error", TEXT(msg))
    );

    app_send_json(res, status, json);
}

int app_valid_username(const char *username)
{
    if (!username)
    {
        return 0;
    }

    size_t len = strlen(username);

    if (len == 0 || len >= APP_USERNAME_LEN)
    {
        return 0;
    }

    for (size_t i = 0; i < len; i++)
    {
        unsigned char c = (unsigned char)username[i];

        if (!isalnum(c) && c != '_' && c != '-')
        {
            return 0;
        }
    }

    return 1;
}
