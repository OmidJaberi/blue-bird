#ifndef APP_WS_H
#define APP_WS_H

#include <blue-bird/web/websocket/websocket.h>
#include <blue-bird/error/error.h>

/*
 * Websocket protocol (JSON text frames):
 *
 * Client -> Server
 *   {"type":"auth","session_id":"..."}
 *   {"type":"message","to":"<username>","body":"<text>"}
 *
 * Server -> Client
 *   {"type":"auth_ok","username":"..."}
 *   {"type":"auth_error","error":"..."}
 *   {"type":"message","id":"...","from":"...","to":"...","body":"...","created_at":123}
 *   {"type":"error","error":"..."}
 *
 * A connection must send "auth" as its first message before it is allowed
 * to send chat messages. Until then, all other message types are rejected.
 */
bb_error_t chat_ws_handler(bb_websocket_t *ws, const bb_ws_message_t *message);

/* Is the given username currently connected over a live websocket? */
int chat_is_online(const char *username);

#endif //APP_WS_H
