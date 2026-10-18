#include "blue-bird/utils/random.h"

#include <errno.h>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/random.h>
#endif
#endif

#if !defined(_WIN32)
static int read_urandom(unsigned char *buf, size_t len)
{
    int fd = open("/dev/urandom", O_RDONLY);
    size_t filled = 0;

    if (fd < 0)
        return 1;

    while (filled < len)
    {
        ssize_t n = read(fd, buf + filled, len - filled);

        if (n < 0 && errno == EINTR)
            continue;

        if (n <= 0)
        {
            close(fd);
            return 1;
        }

        filled += (size_t)n;
    }

    close(fd);
    return 0;
}
#endif

int bb_random_bytes(void *buf, size_t len)
{
    if (!buf)
        return 1;

    if (len == 0)
        return 0;

#if defined(_WIN32)
    return BCryptGenRandom(NULL, (PUCHAR)buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0 ? 0 : 1;
#else
    unsigned char *p = (unsigned char *)buf;
#if defined(__linux__)
    size_t filled = 0;

    while (filled < len)
    {
        ssize_t n = getrandom(p + filled, len - filled, 0);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;

            return read_urandom(p, len); /* e.g. ENOSYS on very old kernels */
        }

        filled += (size_t)n;
    }

    return 0;
#else
    return read_urandom(p, len);
#endif
#endif
}
