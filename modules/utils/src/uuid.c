#include "utils/uuid.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static int bb_uuid_random_bytes(uint8_t *buf, size_t len)
{
#if defined(_WIN32)
    return BCryptGenRandom(NULL,
                            buf,
                            (ULONG)len,
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0
           ? 0
           : 1;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0)
        return 1;

    ssize_t r = read(fd, buf, len);
    close(fd);

    return r == (ssize_t)len ? 0 : 1;
#endif
}

/* ---------- UUID v4 ---------- */

int bb_uuid_v4(uint8_t out[16])
{
    if (!out)
        return 1;

    if (bb_uuid_random_bytes(out, 16) != 0)
        return 1;

    /* Set version (4) */
    out[6] = (out[6] & 0x0F) | 0x40;

    /* Set variant (RFC 4122) */
    out[8] = (out[8] & 0x3F) | 0x80;

    return 0;
}

int bb_uuid_to_string(const uint8_t uuid[16],
                      char out[BB_UUID_BUF_LEN])
{
    if (!uuid || !out)
        return 1;

    int written = snprintf(out,
                           BB_UUID_BUF_LEN,
                           "%02x%02x%02x%02x-"
                           "%02x%02x-"
                           "%02x%02x-"
                           "%02x%02x-"
                           "%02x%02x%02x%02x%02x%02x",
                           uuid[0], uuid[1], uuid[2], uuid[3],
                           uuid[4], uuid[5],
                           uuid[6], uuid[7],
                           uuid[8], uuid[9],
                           uuid[10], uuid[11], uuid[12],
                           uuid[13], uuid[14], uuid[15]);

    return (written == BB_UUID_STR_LEN) ? 0 : 1;
}

int bb_uuid_v4_string(char out[BB_UUID_BUF_LEN])
{
    uint8_t uuid[16];

    if (bb_uuid_v4(uuid) != 0)
        return 1;

    return bb_uuid_to_string(uuid, out);
}