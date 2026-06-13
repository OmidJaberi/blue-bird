#include "blue-bird/web/http/response.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void test_response_basic(void)
{
    bb_response_t *res = bb_response_create();;
    char *buffer;
    size_t size;

    bb_response_set_status(res, 200);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, "Hello there!");

    bb_response_serialize(res, &buffer, &size);
    
    assert(strstr(buffer, "HTTP/1.1 200 OK") != NULL);
    assert(strstr(buffer, "Content-Type: text/plain") != NULL);
    assert(strstr(buffer, "Content-Length: 12") != NULL);
    assert(strstr(buffer, "Hello there!") != NULL);

    free(buffer);
    bb_response_destroy(res);
}

void test_response_multiple_headers(void)
{
    bb_response_t *res = bb_response_create();;
    char *buffer;
    size_t size;

    bb_response_set_status(res, 201);
    bb_response_set_header(res, "Content-Type", "application/json");
    bb_response_set_header(res, "X-Custom", "Blue-Bird");
    bb_response_set_body(res, "{\"ok\":true}");

    bb_response_serialize(res, &buffer, &size);
    
    assert(strstr(buffer, "HTTP/1.1 201 Created") != NULL);
    assert(strstr(buffer, "Content-Type: application/json") != NULL);
    assert(strstr(buffer, "X-Custom: Blue-Bird") != NULL);
    assert(strstr(buffer, "Content-Length: 11") != NULL);
    assert(strstr(buffer, "{\"ok\":true}") != NULL);

    free(buffer);
    bb_response_destroy(res);
}

void test_response_empty_body(void)
{
    bb_response_t *res = bb_response_create();;
    char *buffer;
    size_t size;

    bb_response_set_status(res, 204);

    bb_response_serialize(res, &buffer, &size);
   
    assert(strstr(buffer, "HTTP/1.1 204 No Content") != NULL);
    assert(strstr(buffer, "Content-Length: 0") != NULL);

    free(buffer);
    bb_response_destroy(res);
}

void test_response_large_body(void)
{
    bb_response_t *res = bb_response_create();;
    char *buffer;
    size_t size;

    size_t large_size = 50 * 1000 + 1;
    char *large_body = malloc(large_size); // 50 KB

    for (unsigned long i = 0; i < large_size - 1; i++)
        large_body[i] = 'A';
    large_body[large_size - 1] = '\0';

    bb_response_set_status(res, 200);
    bb_response_set_header(res, "Content-Type", "text/plain");
    bb_response_set_body(res, large_body);

    bb_response_serialize(res, &buffer, &size);

    assert(strstr(buffer, "Content-Length: 50000") != NULL);
    assert(buffer[size - 2] == 'A');

    free(buffer);
    bb_response_destroy(res);
}

int main(void)
{
    printf("Running Response tests...\n");
    test_response_basic();
    test_response_multiple_headers();
    test_response_empty_body();
    test_response_large_body();
    printf("All tests passed.\n");
    return 0;
}
