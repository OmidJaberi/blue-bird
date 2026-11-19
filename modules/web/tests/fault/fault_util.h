#ifndef BB_FAULT_UTIL_H
#define BB_FAULT_UTIL_H

/* Small helpers shared by the failure-injection tests. POSIX only. */

#include <blue-bird/error/assert.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>

/* A failed check names the injection point it happened at, so a sweep failure
 * says which allocation/registration to look at. */
static inline void fault_check(bool ok, const char *what, unsigned n)
{
    if (!ok)
    {
        fprintf(stderr, "\tFAILED at injection point %u: %s\n", n, what);
        BB_ASSERT(0 && "failure-injection check failed");
    }
}

/* Open descriptors below 256: enough to notice a leaked socket or poller. */
static inline int fault_count_open_fds(void)
{
    int open_fds = 0;
    for (int fd = 0; fd < 256; fd++)
    {
        if (fcntl(fd, F_GETFD) != -1)
        {
            open_fds++;
        }
    }
    return open_fds;
}

#endif /* BB_FAULT_UTIL_H */
