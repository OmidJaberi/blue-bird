#include "core/middleware.h"
#include <stdio.h>

static Middleware middleware_list[MAX_MIDDLEWARE];
static int middleware_count = 0;

void use_middleware(Middleware mw)
{
    if (middleware_count >= MAX_MIDDLEWARE)
    {
        fprintf(stderr, "Max middleware reached!\n");
        return;
    }
    middleware_list[middleware_count++] = mw;
}

int run_middleware(Request *req, Response *res)
{
    for (int i = 0; i < middleware_count; i++)
    {
        int result = middleware_list[i](req, res);
        if (result != 0)
            return result;
    }
    return 0;
}

// Just for unit testing, temporaty:
void clear_middleware()
{
    middleware_count = 0;
}
