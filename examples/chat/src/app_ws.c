#include "app_ws.h"
#include "app_repo.h"
#include "app_util.h"

#include <blue-bird/log/log.h>
#include <blue-bird/utils/json.h>
#include <blue-bird/utils/uuid.h>
#include <blue-bird/security/session.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * Online presence registry
 *
 * The public websocket API does not expose connect/disconnect
 * events (only text/binary messages reach the handler), so a
 * connection identifies itself with an explicit "auth" message
 * right after opening. Stale entries are lazily evicted whenever
 * a send to them fails.
 * ============================================================ */

typedef struct online_node {
    char username[APP_USERNAME_LEN];
    bb_websocket_t *ws;
    struct online_node *next;
} online_node_t;

static online_node_t *online_list = NULL;

static void online_remove_by_ws(bb_websocket_t *ws)
{
    online_node_t **cur = &online_list;

    while (*cur)
    {
        if ((*cur)->ws == ws)
        {
            online_node_t *dead = *cur;
            *cur = dead->next;
            free(dead);
        }
        else
        {
            cur = &(*cur)->next;
        }
    }
}

static void online_remove_by_username(const char *username)
{
    online_node_t **cur = &online_list;

    while (*cur)
    {
        if (strcmp((*cur)->username, username) == 0)
        {
            online_node_t *dead = *cur;
            *cur = dead->next;
            free(dead);
        }
        else
        {
            cur = &(*cur)->next;
        }
    }
}

static void online_add(const char *username, bb_websocket_t *ws)
{
    online_remove_by_ws(ws);
    online_remove_by_username(username);

    online_node_t *node = malloc(sizeof(*node));
    if (!node)
    {
        return;
    }

    strncpy(node->username, username, sizeof(node->username) - 1);
    node->username[sizeof(node->username) - 1] = '\0';
    node->ws = ws;
    node->next = online_list;
    online_list = node;
}

static bb_websocket_t *online_find(const char *username)
{
    for (online_node_t *n = online_list; n; n = n->next)
    {
        if (strcmp(n->username, username) == 0)
        {
            return n->ws;
        }
    }

    return NULL;
}

static const char *online_find_username(bb_websocket_t *ws)
{
    for (online_node_t *n = online_list; n; n = n->next)
    {
        if (n->ws == ws)
        {
            return n->username;
        }
    }

    return NULL;
}

int chat_is_online(const char *username)
{
    return online_find(username) != NULL;
}

/* ============================================================
 * Outgoing message helpers
 * ============================================================ */

static void ws_send_json(bb_websocket_t *ws, bb_json_t *json)
{
    char *buf = NULL;
    int size = 0;

    bb_json_serialize(json, &buf, &size);
    bb_json_destroy(json);

    if (!buf)
    {
        return;
    }

    if (BB_FAILED(bb_websocket_send_text(ws, buf)))
    {
        /* Connection is dead; forget about it so we stop routing to it. */
        const char *username = online_find_username(ws);
        if (username)
        {
            BB_LOG_INFO("[chat] dropping stale connection for %s\n", username);
        }
        online_remove_by_ws(ws);
    }

    free(buf);
}

static void ws_send_error(bb_websocket_t *ws, const char *msg)
{
    ws_send_json(ws, OBJ(
        KEY("type", TEXT("error")),
        KEY("error", TEXT(msg))
    ));
}

/* ============================================================
 * Message handling
 * ============================================================ */

static void handle_auth(bb_websocket_t *ws, bb_json_t *json)
{
    const char *session_id = app_json_get_string(json, "session_id");

    if (!session_id)
    {
        ws_send_json(ws, OBJ(
            KEY("type", TEXT("auth_error")),
            KEY("error", TEXT("missing session_id"))
        ));
        return;
    }

    bb_session_t session;

    if (BB_FAILED(bb_session_get(session_id, &session)))
    {
        ws_send_json(ws, OBJ(
            KEY("type", TEXT("auth_error")),
            KEY("error", TEXT("invalid or expired session"))
        ));
        return;
    }

    online_add(session.user_id, ws);

    ws_send_json(ws, OBJ(
        KEY("type", TEXT("auth_ok")),
        KEY("username", TEXT(session.user_id))
    ));

    BB_LOG_INFO("[chat] %s connected\n", session.user_id);
}

static void handle_chat_message(bb_websocket_t *ws, bb_json_t *json)
{
    const char *from = online_find_username(ws);

    if (!from)
    {
        ws_send_error(ws, "not authenticated");
        return;
    }

    const char *to = app_json_get_string(json, "to");
    const char *body = app_json_get_string(json, "body");

    if (!to || !body || body[0] == '\0')
    {
        ws_send_error(ws, "message requires 'to' and non-empty 'body'");
        return;
    }

    if (!user_exists(&global_user_repo, to))
    {
        ws_send_error(ws, "recipient does not exist");
        return;
    }

    Message m = {0};
    bb_uuid_v4_string(m.id);
    strncpy(m.from_user, from, sizeof(m.from_user) - 1);
    strncpy(m.to_user, to, sizeof(m.to_user) - 1);
    strncpy(m.body, body, sizeof(m.body) - 1);
    m.created_at = (int) time(NULL);

    if (message_insert(&global_message_repo, &m) != 0)
    {
        ws_send_error(ws, "failed to store message");
        return;
    }

    bb_json_t *payload = OBJ(
        KEY("type", TEXT("message")),
        KEY("id", TEXT(m.id)),
        KEY("from", TEXT(m.from_user)),
        KEY("to", TEXT(m.to_user)),
        KEY("body", TEXT(m.body)),
        KEY("created_at", INT(m.created_at))
    );

    /* Echo back to the sender so their UI can confirm delivery/id. */
    char *buf = NULL;
    int size = 0;
    bb_json_serialize(payload, &buf, &size);

    if (buf)
    {
        bb_websocket_send_text(ws, buf);
    }

    /* Forward live to the recipient, if they're online. */
    bb_websocket_t *peer_ws = online_find(to);

    if (peer_ws && peer_ws != ws && buf)
    {
        if (BB_FAILED(bb_websocket_send_text(peer_ws, buf)))
        {
            online_remove_by_ws(peer_ws);
        }
    }

    free(buf);
    bb_json_destroy(payload);
}

bb_error_t chat_ws_handler(bb_websocket_t *ws, const bb_ws_message_t *message)
{
    if (message->type != BB_WS_MESSAGE_TEXT)
    {
        return BB_SUCCESS();
    }

    char *raw = strndup((const char *) message->data, message->length);
    if (!raw)
    {
        return BB_ERROR(BB_ERR_ALLOC, "allocation failed");
    }

    bb_json_t *json = bb_json_parse(raw);

    if (!json || bb_json_get_type(json) != BB_JSON_OBJECT)
    {
        free(raw);
        ws_send_error(ws, "invalid JSON");
        return BB_SUCCESS();
    }

    free(raw);

    const char *type = app_json_get_string(json, "type");

    if (type && strcmp(type, "auth") == 0)
    {
        handle_auth(ws, json);
    }
    else if (type && strcmp(type, "message") == 0)
    {
        handle_chat_message(ws, json);
    }
    else
    {
        ws_send_error(ws, "unknown message type");
    }

    bb_json_destroy(json);

    return BB_SUCCESS();
}
