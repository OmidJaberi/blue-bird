#include "blue-bird/web/websocket/message.h"

#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <string.h>

void test_message_init_text(void)
{
    printf("\tTesting text message initialization...\n");

    bb_ws_message_t message;

    const char *text = "hello";

    bb_ws_message_init(&message, BB_WS_MESSAGE_TEXT, text, strlen(text));

    BB_ASSERT(message.type == BB_WS_MESSAGE_TEXT);

    BB_ASSERT(message.data == text);

    BB_ASSERT(message.length == strlen(text));
}

void test_message_init_binary(void)
{
    printf("\tTesting binary message initialization...\n");

    bb_ws_message_t message;

    unsigned char data[] =
    {
        0x01,
        0x02,
        0x03
    };

    bb_ws_message_init(&message, BB_WS_MESSAGE_BINARY, data, sizeof(data));

    BB_ASSERT(message.type == BB_WS_MESSAGE_BINARY);

    BB_ASSERT(message.data == data);

    BB_ASSERT(message.length == sizeof(data));
}

void test_message_destroy(void)
{
    printf("\tTesting message destroy...\n");

    bb_ws_message_t message;

    bb_ws_message_init(&message, BB_WS_MESSAGE_TEXT, "hello", 5);

    bb_ws_message_destroy(&message);

    /*
     * Currently a no-op.
     * This test exists so future
     * ownership changes don't
     * break silently.
     */
}

int main(void)
{
    printf("Running WebSocket Message tests...\n");

    test_message_init_text();

    test_message_init_binary();

    test_message_destroy();

    printf("All tests passed.\n");

    return 0;
}
