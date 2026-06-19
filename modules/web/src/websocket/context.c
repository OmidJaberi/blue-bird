#include "blue-bird/web/websocket/context.h"

#include "websocket/context.h"

#include <string.h>

bb_error_t bb_ws_send_text(bb_ws_context_t *ctx, const char *text)
{
    if (!ctx || !text)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }

    return bb_websocket_send_text(ctx->websocket, text);
}

bb_error_t bb_ws_send_binary(bb_ws_context_t *ctx, const void *data, size_t length)
{
    if (!ctx || !data)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguments");
    }

    return bb_websocket_send_binary(
        ctx->websocket,
        data,
        length
    );
}

bb_error_t bb_ws_close(bb_ws_context_t *ctx)
{
    if (!ctx)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid context");
    }

    return bb_websocket_send_close(ctx->websocket);
}

void *bb_ws_userdata(bb_ws_context_t *ctx)
{
    if (!ctx || !ctx->websocket)
    {
        return NULL;
    }

    return ctx->websocket->connection->userdata;
}

void bb_ws_set_userdata(bb_ws_context_t *ctx, void *userdata)
{
    if (!ctx || !ctx->websocket)
    {
        return;
    }

    ctx->websocket->connection->userdata =userdata;
}
