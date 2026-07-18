#ifndef BB_PLATFORM_H
#define BB_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif


#include <errno.h>
#include <string.h>

#if defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <BaseTsd.h>
typedef SSIZE_T ssize_t;

#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

typedef SOCKET bb_socket_t;
#define BB_INVALID_SOCKET INVALID_SOCKET
#define BB_SOCKET_ERROR   SOCKET_ERROR
#define MSG_NOSIGNAL      0

#else /* POSIX */

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>

typedef int bb_socket_t;
#define BB_INVALID_SOCKET (-1)
#define BB_SOCKET_ERROR   (-1)

#endif

/*
 * Must be called once before any socket API is used (no-op on POSIX,
 * wraps WSAStartup on Windows). Returns 0 on success, -1 on failure.
 */
int bb_platform_net_init(void);

/*
 * Must be called once at shutdown (no-op on POSIX, wraps WSACleanup
 * on Windows).
 */
void bb_platform_net_cleanup(void);

/* Closes a socket handle. Wraps close()/closesocket(). */
int bb_socket_close(bb_socket_t sock);

/*
 * Puts a socket into non-blocking mode.
 * Wraps fcntl(F_SETFL, O_NONBLOCK) / ioctlsocket(FIONBIO).
 * Returns 0 on success, -1 on failure.
 */
int bb_socket_set_nonblocking(bb_socket_t sock);

/*
 * Returns the last socket error in a platform-neutral way
 * (errno on POSIX, WSAGetLastError() on Windows).
 */
int bb_socket_last_error(void);

/*
 * Sleeps for the specified number of microseconds.
 * Wraps usleep() on POSIX and Sleep() (or a high-resolution wait)
 * on Windows.
 */
void bb_usleep(unsigned int usec);

/* True if `sock` is not a valid socket handle. */
static inline int bb_socket_is_invalid(bb_socket_t sock)
{
    return sock == BB_INVALID_SOCKET;
}

static inline char *bb_strndup(const char *s, size_t n)
{
#if defined(_WIN32)
    size_t len = strnlen(s, n);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len);
    copy[len] = '\0';
    return copy;
#else /* POSIX */
    return strndup(s, n);
#endif
}

#if defined(_WIN32)
static inline int socketpair(int domain, int type, int protocol, int sv[2])
{
    (void)domain;
    (void)protocol;

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
        if (client != INVALID_SOCKET) closesocket(client);
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

static inline int bb_socket_would_block(void)
{
#if defined(_WIN32)
    return bb_socket_last_error() == WSAEWOULDBLOCK;
#else
    int err = bb_socket_last_error();
    return err == EAGAIN || err == EWOULDBLOCK;
#endif
}


#ifdef __cplusplus
}
#endif

#endif /* BB_PLATFORM_H */
