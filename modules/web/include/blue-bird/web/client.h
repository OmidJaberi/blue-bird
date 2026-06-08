#ifndef BB_CLIENT_H
#define BB_CLIENT_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/web/http/request.h"
#include "blue-bird/web/http/response.h"
#include "blue-bird/error/error.h"

typedef struct bb_client bb_client_t;

bb_client_t *bb_client_create(void);
void bb_client_destroy(bb_client_t *client);
void bb_client_reset(bb_client_t *client);

bb_request_t *bb_client_get_request(bb_client_t *client);
bb_response_t *bb_client_get_response(bb_client_t *client);


bb_error_t bb_client_connect(bb_client_t *client);
bb_error_t bb_client_send(bb_client_t *client);
bb_error_t bb_client_receive(bb_client_t *client);
void bb_client_close(bb_client_t *client);

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


#ifdef __cplusplus
}
#endif

#endif //BB_CLIENT_H
