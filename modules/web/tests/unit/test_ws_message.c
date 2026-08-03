#include "websocket/message_internal.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

void test_message_init_text(void)
{
    printf("\tTesting text message initialization...\n");

    const char *text = "hello";

    bb_ws_message_t *message = bb_ws_message_create(BB_WS_MESSAGE_TEXT, text, strlen(text));

    BB_ASSERT(message->type == BB_WS_MESSAGE_TEXT);

    BB_ASSERT(memcmp(message->data, text, strlen(text)) == 0);

    BB_ASSERT(message->length == strlen(text));

    bb_ws_message_destroy(message);
}

void test_message_init_binary(void)
{
    printf("\tTesting binary message initialization...\n");

    unsigned char data[] =
    {
        0x01,
        0x02,
        0x03
    };

    bb_ws_message_t *message = bb_ws_message_create(BB_WS_MESSAGE_BINARY, data, sizeof(data));

    BB_ASSERT(message->type == BB_WS_MESSAGE_BINARY);

    BB_ASSERT(memcmp(message->data, data, sizeof(data)) == 0);

    BB_ASSERT(message->length == sizeof(data));

    bb_ws_message_destroy(message);
}

int main(void)
{
    printf("Running WebSocket Message tests...\n");

    test_message_init_text();

    test_message_init_binary();

    printf("All tests passed.\n");

    return 0;
}
