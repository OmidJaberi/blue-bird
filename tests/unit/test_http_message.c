#include "core/http/message.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void test_message_basic()
{
    http_message_t msg;
    char buffer[1024];

    init_message(&msg);
    set_message_start_line(&msg, "HTTP/1.1 200 OK");
    set_message_header(&msg, "Content-Type", "text/plain");
    set_message_body(&msg, "Hello there!");

    int len = serialize_message(&msg, buffer, sizeof(buffer));
    
    assert(strstr(buffer, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(buffer, "Content-Type: text/plain") != NULL);
    assert(strstr(buffer, "Content-Length: 12") != NULL);
    assert(strstr(buffer, "Hello there!") != NULL);

    destroy_message(&msg);
}

void test_message_multiple_headers()
{
    http_message_t msg;
    char buffer[1024];

    init_message(&msg);
    set_message_start_line(&msg, "HTTP/1.1 201 Created");
    set_message_header(&msg, "Content-Type", "application/json");
    set_message_header(&msg, "X-Custom", "Blue-Bird");
    set_message_body(&msg, "{\"ok\":true}");

    int len = serialize_message(&msg, buffer, sizeof(buffer));
    
    assert(strstr(buffer, "HTTP/1.1 201 Created") != NULL);
    assert(strstr(buffer, "Content-Type: application/json") != NULL);
    assert(strstr(buffer, "X-Custom: Blue-Bird") != NULL);
    assert(strstr(buffer, "Content-Length: 11") != NULL);
    assert(strstr(buffer, "{\"ok\":true}") != NULL);

    destroy_message(&msg);
}

void test_message_large_body()
{
    http_message_t msg;
    char buffer[65536];

    size_t large_size = 50 * 1000 + 1;
    char *large_body = malloc(large_size); // 50 KB

    for (int i = 0; i < large_size - 1; i++)
        large_body[i] = 'A';
    large_body[large_size - 1] = '\0';

    init_message(&msg);
    set_message_start_line(&msg, "HTTP/1.1 200 OK");
    set_message_header(&msg, "Content-Type", "text/plain");
    set_message_body(&msg, large_body);

    int len = serialize_message(&msg, buffer, sizeof(buffer));

    assert(strstr(buffer, "Content-Length: 50000") != NULL);
    assert(buffer[len - 1] == 'A');

    destroy_message(&msg);
}

int main()
{
    printf("Running HTTP Message tests...\n");
    test_message_basic();
    test_message_multiple_headers();
    test_message_large_body();
    printf("All tests passed.\n");
    return 0;
}
