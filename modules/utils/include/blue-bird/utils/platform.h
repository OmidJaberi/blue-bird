#ifndef BB_PLATFORM_H
#define BB_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* --------------------------------------------------------------------- */
/* Platform-specific includes, types, and macros                         */
/* --------------------------------------------------------------------- */

#if defined(_WIN32)

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <BaseTsd.h>

typedef SSIZE_T ssize_t;
typedef SOCKET  bb_socket_t;

#define BB_INVALID_SOCKET INVALID_SOCKET
#define BB_SOCKET_ERROR    SOCKET_ERROR
#define MSG_NOSIGNAL       0

#define strcasecmp  _stricmp
#define strncasecmp _strnicmp

#else /* POSIX */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

typedef int bb_socket_t;

#define BB_INVALID_SOCKET (-1)
#define BB_SOCKET_ERROR   (-1)

#endif

/* --------------------------------------------------------------------- */
/* Lifecycle                                                              */
/* --------------------------------------------------------------------- */

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

/* --------------------------------------------------------------------- */
/* Socket helpers                                                         */
/* --------------------------------------------------------------------- */

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

/* True if the last socket error indicates a would-block condition. */
static inline int bb_socket_would_block(void)
{
#if defined(_WIN32)
    return bb_socket_last_error() == WSAEWOULDBLOCK;
#else
    int err = bb_socket_last_error();
    return err == EAGAIN || err == EWOULDBLOCK;
#endif
}

/* --------------------------------------------------------------------- */
/* Misc helpers                                                           */
/* --------------------------------------------------------------------- */

/*
 * Sleeps for the specified number of microseconds.
 * Wraps usleep() on POSIX and Sleep() (or a high-resolution wait)
 * on Windows.
 */
void bb_usleep(unsigned int usec);

/*
 * strndup() is not available on all platforms (notably MSVC), so we
 * provide a portable version. Behaves like POSIX strndup().
 */
char *bb_strndup(const char *s, size_t n);

/* --------------------------------------------------------------------- */
/* Compatibility shims                                                    */
/* --------------------------------------------------------------------- */

#if defined(_WIN32)
/*
 * Minimal socketpair() replacement for Windows, sufficient for
 * loopback-only use within this codebase. `domain` and `protocol`
 * are ignored; only AF_INET pairs are created, over loopback.
 */
int socketpair(int domain, int type, int protocol, int sv[2]);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BB_PLATFORM_H */
