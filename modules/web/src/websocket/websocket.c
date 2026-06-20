#include "blue-bird/web/websocket/websocket.h"
#include "blue-bird/error/error.h"
#include "blue-bird/utils/encoding.h"
#include "blue-bird/utils/hash.h"

#include <stdlib.h>
#include <string.h>

bool bb_websocket_is_upgrade_request(bb_request_t *req)
{
    if (!req)
    {
        return false;
    }

    const char *upgrade = bb_request_get_header(req, "Upgrade");

    const char *connection = bb_request_get_header(req, "Connection");

    const char *key = bb_request_get_header(req, "Sec-WebSocket-Key");

    const char *version = bb_request_get_header(req, "Sec-WebSocket-Version");

    if (!upgrade || !connection || !key || !version)
    {
        return false;
    }

    if (strcasecmp(upgrade, "websocket") != 0)
    {
        return false;
    }

    if (strstr(connection, "Upgrade") == NULL && strstr(connection, "upgrade") == NULL)
    {
        return false;
    }

    if (strcmp(version, "13") != 0)
    {
        return false;
    }

    return true;
}

static const char BB_WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

char *bb_websocket_accept_key(const char *client_key)
{
    if (!client_key)
    {
        return NULL;
    }

    size_t key_len = strlen(client_key);
    size_t guid_len = strlen(BB_WS_GUID);

    char *combined = malloc(key_len + guid_len + 1);

    if (!combined)
    {
        return NULL;
    }

    memcpy(combined, client_key, key_len);

    memcpy(combined + key_len, BB_WS_GUID, guid_len + 1);

    unsigned char digest[BB_SHA1_DIGEST_LENGTH];

    bb_sha1(combined, key_len + guid_len, digest);

    free(combined);

    return bb_base64_encode(digest, BB_SHA1_DIGEST_LENGTH);
}

bb_error_t bb_websocket_accept(bb_request_t *req, bb_response_t *res)
{
    if (!req || !res)
    {
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Invalid request or response");
    }

    if (!bb_websocket_is_upgrade_request(req))
    {
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Not a websocket upgrade request");
    }

    const char *client_key = bb_request_get_header(req, "Sec-WebSocket-Key");

    if (!client_key)
    {
        return BB_ERROR(BB_ERR_BAD_REQUEST, "Missing Sec-WebSocket-Key");
    }

    char *accept_key = bb_websocket_accept_key(client_key);

    if (!accept_key)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Failed to generate accept key");
    }

    bb_response_set_status(res, 101);

    bb_response_set_header(res, "Upgrade", "websocket");

    bb_response_set_header(res, "Connection", "Upgrade");

    bb_response_set_header(res, "Sec-WebSocket-Accept", accept_key);

    free(accept_key);

    return BB_SUCCESS();
}
