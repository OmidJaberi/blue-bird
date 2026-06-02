#include "blue-bird/web/http/message.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void test_message_basic(void)
{
    bb_http_message_t *msg = bb_message_create();
    char *buffer;
    size_t size;

    bb_message_set_start_line(msg, "HTTP/1.1 200 OK");
    bb_message_set_header(msg, "Content-Type", "text/plain");
    bb_message_set_body(msg, "Hello there!");

    bb_message_serialize(msg, &buffer, &size);
    
    assert(strstr(buffer, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(buffer, "Content-Type: text/plain") != NULL);
    assert(strstr(buffer, "Content-Length: 12") != NULL);
    assert(strstr(buffer, "Hello there!") != NULL);

    free(buffer);
    bb_message_destroy(msg);
}

void test_message_multiple_headers(void)
{
    bb_http_message_t *msg = bb_message_create();
    char *buffer;
    size_t size;

    bb_message_set_start_line(msg, "HTTP/1.1 201 Created");
    bb_message_set_header(msg, "Content-Type", "application/json");
    bb_message_set_header(msg, "X-Custom", "Blue-Bird");
    bb_message_set_body(msg, "{\"ok\":true}");

    bb_message_serialize(msg, &buffer, &size);
    
    assert(strstr(buffer, "HTTP/1.1 201 Created") != NULL);
    assert(strstr(buffer, "Content-Type: application/json") != NULL);
    assert(strstr(buffer, "X-Custom: Blue-Bird") != NULL);
    assert(strstr(buffer, "Content-Length: 11") != NULL);
    assert(strstr(buffer, "{\"ok\":true}") != NULL);

    free(buffer);
    bb_message_destroy(msg);
}

void test_message_large_body(void)
{
    bb_http_message_t *msg = bb_message_create();
    char *buffer;
    size_t size;

    size_t large_size = 50 * 1000 + 1;
    char *large_body = malloc(large_size); // 50 KB

    for (unsigned long i = 0; i < large_size - 1; i++)
        large_body[i] = 'A';
    large_body[large_size - 1] = '\0';

    bb_message_set_start_line(msg, "HTTP/1.1 200 OK");
    bb_message_set_header(msg, "Content-Type", "text/plain");
    bb_message_set_body(msg, large_body);

    bb_message_serialize(msg, &buffer, &size);

    assert(strstr(buffer, "Content-Length: 50000") != NULL);
    assert(buffer[size - 2] == 'A');

    free(buffer);
    bb_message_destroy(msg);
}

int main(void)
{
    printf("Running HTTP Message tests...\n");
    test_message_basic();
    test_message_multiple_headers();
    test_message_large_body();
    printf("All tests passed.\n");
    return 0;
}
