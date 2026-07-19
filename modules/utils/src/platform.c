#include "blue-bird/utils/platform.h"

#include <signal.h>

/* --------------------------------------------------------------------- */
/* Lifecycle                                                              */
/* --------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------- */
/* Socket helpers                                                         */
/* --------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------- */
/* Misc helpers                                                           */
/* --------------------------------------------------------------------- */

void bb_usleep(unsigned int usec)
{
#if defined(_WIN32)
    /* Sleep() is millisecond resolution. */
    DWORD msec = (usec + 999) / 1000;
    Sleep(msec);
#else
    usleep(usec);
#endif
}

char *bb_strndup(const char *s, size_t n)
{
#if defined(_WIN32)
    size_t len = strnlen(s, n);
    char *copy = malloc(len + 1);
    if (!copy)
    {
        return NULL;
    }
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
#else /* POSIX */
    return strndup(s, n);
#endif
}

/* --------------------------------------------------------------------- */
/* Compatibility shims                                                    */
/* --------------------------------------------------------------------- */

#if defined(_WIN32)
int socketpair(int domain, int type, int protocol, int sv[2])
{
    (void)domain;
    (void)protocol;

    /*
     * Not thread-safe: assumes bb_platform_net_init() has already been
     * called, or that this is invoked from a single-threaded context
     * before other sockets are created.
     */
    static int wsa_ready = 0;
    if (!wsa_ready)
    {
        if (bb_platform_net_init() != 0)
        {
            return -1;
        }
        wsa_ready = 1;
    }

    SOCKET listener = socket(AF_INET, type, 0);
    if (listener == INVALID_SOCKET)
    {
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(listener, 1) == SOCKET_ERROR)
    {
        closesocket(listener);
        return -1;
    }

    int len = sizeof(addr);
    if (getsockname(listener, (struct sockaddr*)&addr, &len) == SOCKET_ERROR)
    {
        closesocket(listener);
        return -1;
    }

    SOCKET client = socket(AF_INET, type, 0);
    if (client == INVALID_SOCKET ||
        connect(client, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(listener);
        if (client != INVALID_SOCKET)
        {
            closesocket(client);
        }
        return -1;
    }

    SOCKET accepted = accept(listener, NULL, NULL);
    closesocket(listener);

    if (accepted == INVALID_SOCKET)
    {
        closesocket(client);
        return -1;
    }

    sv[0] = (int)client;
    sv[1] = (int)accepted;
    return 0;
}
#endif
