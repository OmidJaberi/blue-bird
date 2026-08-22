#ifndef BB_PLATFORM_H
#define BB_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* --------------------------------------------------------------------- */
/* Platform-specific includes, types, and macros                         */
/* --------------------------------------------------------------------- */

#if defined(_WIN32)

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600 /* Vista+, required for WSAPoll() */
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <BaseTsd.h>
#include <direct.h>

typedef SSIZE_T ssize_t;
typedef SOCKET  bb_socket_t;

#define BB_INVALID_SOCKET INVALID_SOCKET
#define BB_SOCKET_ERROR    SOCKET_ERROR
#define MSG_NOSIGNAL       0

#else /* POSIX */

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int bb_socket_t;

#define BB_INVALID_SOCKET (-1)
#define BB_SOCKET_ERROR   (-1)

#endif

/* --------------------------------------------------------------------- */
/* Poller backend selection                                              */
/* --------------------------------------------------------------------- */

/*
 * Selects which readiness-notification syscall the runtime poller (see
 * runtime/internal/poller.h) is built against. Exactly one of these is
 * defined. All three are readiness-based, like select() -- Windows'
 * IOCP is deliberately not used here since it's completion-based and
 * would need a different poller interface entirely, not a drop-in swap.
 */
#if defined(_WIN32)
#define BB_POLLER_BACKEND_POLL 1 /* WSAPoll() */
#elif defined(__linux__)
#define BB_POLLER_BACKEND_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
      defined(__OpenBSD__) || defined(__DragonFly__)
#define BB_POLLER_BACKEND_KQUEUE 1
#else
#define BB_POLLER_BACKEND_POLL 1 /* generic POSIX poll() fallback */
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

/**
 * Returns true if the last socket error indicates that the peer has
 * disconnected and the connection can no longer be used.
 */
bool bb_socket_connection_closed(void);

/* True if `sock` is not a valid socket handle. */
static inline int bb_socket_is_invalid(bb_socket_t sock)
{
    return sock == BB_INVALID_SOCKET;
}

/*
 * True if `fd` can be handed to the runtime poller. None of the current
 * backends (epoll, kqueue, WSAPoll/poll) impose a numeric ceiling on the
 * fd value the way select()'s fd_set bitmap does, so this only needs to
 * reject invalid handles.
 */
static inline int bb_poller_fd_supported(bb_socket_t fd)
{
    return !bb_socket_is_invalid(fd);
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
/* Misc helpers                                                          */
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
char *bb_strdup(const char *s);

static inline int bb_strcasecmp(const char *s1, const char *s2)
{
#ifdef _WIN32
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

static inline int bb_strncasecmp(const char *s1, const char *s2, size_t n)
{
#ifdef _WIN32
    return _strnicmp(s1, s2, n);
#else
    return strncasecmp(s1, s2, n);
#endif
}

static inline void bb_mkdir(const char *path)
{
    #ifdef _WIN32
        _mkdir(path);
    #else
        mkdir(path, 0755);
    #endif
}

/* --------------------------------------------------------------------- */
/* Compatibility shims                                                    */
/* --------------------------------------------------------------------- */

#if defined(_WIN32)
/*
 * Minimal socketpair() replacement for Windows, sufficient for
 * loopback-only use within this codebase. `domain` and `protocol`
 * are ignored; only AF_INET pairs are created, over loopback.
 */
int socketpair(int domain, int type, int protocol, bb_socket_t sv[2]);
#endif

#ifdef __cplusplus
}
#endif

#endif /* BB_PLATFORM_H */
