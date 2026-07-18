#ifndef BB_PLATFORM_H
#define BB_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>

#if defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

typedef SOCKET bb_socket_t;
#define BB_INVALID_SOCKET INVALID_SOCKET
#define BB_SOCKET_ERROR   SOCKET_ERROR

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

/* True if `sock` is not a valid socket handle. */
static inline int bb_socket_is_invalid(bb_socket_t sock)
{
    return sock == BB_INVALID_SOCKET;
}

#ifdef __cplusplus
}
#endif

#endif /* BB_PLATFORM_H */
