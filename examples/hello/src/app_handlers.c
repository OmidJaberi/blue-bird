#include "app_handlers.h"
#include <stdio.h>
#include <string.h>

int hello_get_handler(Request *req, Response *res)
{
    set_header(res, "Content-Type", "text/plain");
    const char *name = get_query_param(req, "name");
    if (name)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "Hello %s, via GET!", name);
        set_body(res, buf);
    }
    else
        set_body(res, "Hello via GET!");
}

int hello_post_handler(Request *req, Response *res)
{
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Hello via POST!");
}

int root_handler(Request *req, Response *res)
{
    set_header(res, "Content-Type", "text/plain");
    set_body(res, "Blue-Bird :)");
}

int user_handler(Request *req, Response *res)
{
    const char *user_id = get_param(req, "id");
    set_header(res, "Content-Type", "text/plain");
    char msg[128];
    snprintf(msg, sizeof(msg), "User ID: %s", user_id ? user_id : "unknown");
    set_body(res, msg);
}

int comments_handler(Request *req, Response *res)
{
    const char *user_id = get_param(req, "id");
    set_header(res, "Content-Type", "text/plain");
    char msg[128];
    snprintf(msg, sizeof(msg), "Comments by user: %s", user_id ? user_id : "unknown");
    set_body(res, msg);
}
