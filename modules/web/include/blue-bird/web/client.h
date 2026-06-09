#ifndef BB_CLIENT_H
#define BB_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/runtime/runtime.h"
#include "blue-bird/web/http/request.h"
#include "blue-bird/web/http/response.h"
#include "blue-bird/error/error.h"

typedef struct bb_client bb_client_t;

typedef void (*bb_client_callback_t)(bb_client_t *client, bb_error_t err, void *userdata);

bb_client_t *bb_client_create_on_runtime(bb_runtime_t *runtime);
void bb_client_destroy(bb_client_t *client);
void bb_client_reset(bb_client_t *client);
void bb_client_close(bb_client_t *client);

static inline bb_client_t *bb_client_create(void)
{
    return bb_client_create_on_runtime(bb_runtime_default());
}

bb_request_t *bb_client_get_request(bb_client_t *client);
bb_response_t *bb_client_get_response(bb_client_t *client);

// Sync Client:
bb_error_t bb_client_connect(bb_client_t *client);
bb_error_t bb_client_send(bb_client_t *client);
bb_error_t bb_client_receive(bb_client_t *client);

static inline bb_error_t bb_client_execute(bb_client_t *client)
{
    bb_error_t err = bb_client_connect(client);
    if (BB_FAILED(err))
    {
        return err;
    }

    err = bb_client_send(client);
    if (!BB_FAILED(err))
    {
        err = bb_client_receive(client);
    }

    bb_client_close(client);
    return err;
}

static inline bb_error_t bb_client_get(bb_client_t *client, const char *url)
{
    bb_request_t *req = bb_client_get_request(client);

    bb_client_reset(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);

    return bb_client_execute(client);
}

static inline bb_error_t bb_client_post(bb_client_t *client, const char *url, const char *body)
{
    bb_request_t *req = bb_client_get_request(client);

    bb_client_reset(client);

    bb_request_set_method(req, "POST");
    bb_request_set_url(req, url);
    bb_request_set_body(req, (char *)body);

    return bb_client_execute(client);
}

// Async Client:
void bb_client_execute_async(bb_client_t *client, bb_client_callback_t callback, void *userdata);

static inline void bb_client_get_async(bb_client_t *client, const char *url, bb_client_callback_t cb, void *userdata)
{
    bb_request_t *req = bb_client_get_request(client);

    bb_client_reset(client);

    bb_request_set_method(req, "GET");
    bb_request_set_url(req, url);

    bb_client_execute_async(client, cb, userdata);
}

static inline void bb_client_post_async(bb_client_t *client, const char *url, const char *body, bb_client_callback_t cb, void *userdata)
{
    bb_request_t *req = bb_client_get_request(client);

    bb_client_reset(client);

    bb_request_set_method(req, "POST");
    bb_request_set_url(req, url);
    bb_request_set_body(req, (char *)body);

    bb_client_execute_async(client, cb, userdata);
}


#ifdef __cplusplus
}
#endif

#endif //BB_CLIENT_H
