#include "blue-bird/web/websocket/context.h"

#include "websocket/context_internal.h"

#include <stdlib.h>

bb_ws_context_t *bb_ws_context_create(bb_websocket_t *websocket)
{
    if (!websocket)
    {
        return NULL;
    }

    bb_ws_context_t *ctx =
        malloc(sizeof(*ctx));

    if (!ctx)
    {
        return NULL;
    }

    ctx->websocket = websocket;
    ctx->userdata = NULL;

    return ctx;
}

void bb_ws_context_destroy(bb_ws_context_t *ctx)
{
    free(ctx);
}

bb_error_t bb_ws_send_text(bb_ws_context_t *ctx, const char *text)
{
    if (!ctx || !text)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }

    return bb_websocket_queue_text(ctx->websocket, text);
}

bb_error_t bb_ws_send_binary(bb_ws_context_t *ctx, const void *data, size_t length)
{
    if (!ctx || !data)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }

    return bb_websocket_queue_binary(ctx->websocket, data, length);
}

bb_error_t bb_ws_close(bb_ws_context_t *ctx)
{
    if (!ctx)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid context");
    }

    return bb_websocket_queue_close(ctx->websocket);
}

void *bb_ws_userdata(bb_ws_context_t *ctx)
{
    if (!ctx)
    {
        return NULL;
    }

    return ctx->userdata;
}

void bb_ws_set_userdata(bb_ws_context_t *ctx, void *userdata)
{
    if (!ctx)
    {
        return;
    }

    ctx->userdata = userdata;
}
