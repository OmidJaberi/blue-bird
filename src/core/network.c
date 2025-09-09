#include "core/network.h"
#include <stdio.h>

int init_server(int port)
{
    printf("Blue-Bird server initialized on port %d\n", port);
    return 0;
}

void start_server()
{
    printf("Blue-Bird server started.\n");
    while (1)
    {
        // event loop stuff
    }
}
