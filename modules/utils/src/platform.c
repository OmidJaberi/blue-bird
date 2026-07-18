#include "blue-bird/utils/platform.h"

#include <signal.h>

int bb_platform_net_init(void)
{
#if defined(SIGPIPE) && !defined(_WIN32)
    signal(SIGPIPE, SIG_IGN);
#endif

#if defined(_WIN32)
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0 ? 0 : -1;
#else
    return 0;
#endif
}

void bb_platform_net_cleanup(void)
{
#if defined(_WIN32)
    WSACleanup();
#endif
}

int bb_socket_close(bb_socket_t sock)
{
#if defined(_WIN32)
    return closesocket(sock);
#else
    return close(sock);
#endif
}

int bb_socket_set_nonblocking(bb_socket_t sock)
{
#if defined(_WIN32)
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode) == 0 ? 0 : -1;
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        return -1;
    }
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

int bb_socket_last_error(void)
{
#if defined(_WIN32)
    return WSAGetLastError();
#else
    return errno;
#endif
}
