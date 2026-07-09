#ifndef APP_UTIL_H
#define APP_UTIL_H

#include <blue-bird/web/http/request.h>
#include <blue-bird/web/http/response.h>
#include <blue-bird/utils/json.h>
#include <blue-bird/security/session.h>

#include <stddef.h>

/* Reads a single cookie value from the request's Cookie header.
 * Returns 0 on success, -1 if not present. */
int app_get_cookie(bb_request_t *req, const char *name, char *out, size_t out_size);

/* Sets the session cookie on the response. */
void app_set_session_cookie(bb_response_t *res, const char *session_id);

/* Clears the session cookie on the response. */
void app_clear_session_cookie(bb_response_t *res);

/* Resolves the authenticated user (if any) from the request's session
 * cookie. Returns 0 and fills *session on success, -1 otherwise. */
int app_get_session(bb_request_t *req, bb_session_t *session);

/* Parses the request body as JSON. Returns NULL on invalid/missing JSON. */
bb_json_t *app_parse_body_json(bb_request_t *req);

/* Safely reads a string field from a JSON object. Returns NULL if the key
 * is missing or not a text node (never asserts/aborts on malformed input). */
const char *app_json_get_string(bb_json_t *obj, const char *key);

/* Sends a JSON body with the given status code, then destroys `json`. */
void app_send_json(bb_response_t *res, int status, bb_json_t *json);

/* Sends {"ok":false,"error":"<msg>"} with the given status code. */
void app_send_error(bb_response_t *res, int status, const char *msg);

/* Basic validation: 1-63 chars, alphanumeric, '_' or '-' only. */
int app_valid_username(const char *username);

#endif //APP_UTIL_H
