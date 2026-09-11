/*
 * OS-backed cryptographically secure random number generator.
 *
 * This file intentionally has no dependency on rand()/srand() or any
 * other non-cryptographic PRNG. Security-sensitive callers (password
 * salts, session IDs, etc.) go through _bb_random_bytes()/_bb_random_hex()
 * declared in random.h.
 */

#include <errno.h>
#include <string.h>

#include "../internal/random.h"
#include "blue-bird/security/session.h" /* BB_ERR_RANDOM_FAILED */

#if defined(_WIN32)

#define BB_RANDOM_BACKEND_WINDOWS 1

#include <windows.h>
#include <bcrypt.h>

#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#endif

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#elif defined(__APPLE__) || defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__)

#define BB_RANDOM_BACKEND_GETENTROPY 1

#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>

#elif defined(__linux__)

#define BB_RANDOM_BACKEND_LINUX 1

#include <fcntl.h>
#include <unistd.h>
#include <sys/random.h>

#else

#define BB_RANDOM_BACKEND_URANDOM 1

#include <fcntl.h>
#include <unistd.h>

#endif

#if !defined(BB_RANDOM_BACKEND_WINDOWS)

/*
 * Fallback/base backend shared by POSIX platforms: read raw bytes
 * straight from /dev/urandom. This is a correct CSPRNG source on every
 * mainstream Unix, and is used either directly (BB_RANDOM_BACKEND_URANDOM)
 * or as a fallback when a platform-specific syscall is unavailable/fails.
 */
static bb_error_t random_bytes_urandom(unsigned char *buffer, size_t size)
{
    int fd = open("/dev/urandom", O_RDONLY);

    if (fd < 0)
        return BB_ERROR(BB_ERR_RANDOM_FAILED, "failed to open /dev/urandom");

    size_t filled = 0;

    while (filled < size)
    {
        ssize_t n = read(fd, buffer + filled, size - filled);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;

            close(fd);
            return BB_ERROR(BB_ERR_RANDOM_FAILED, "failed to read /dev/urandom");
        }

        if (n == 0)
        {
            close(fd);
            return BB_ERROR(BB_ERR_RANDOM_FAILED, "unexpected EOF from /dev/urandom");
        }

        filled += (size_t)n;
    }

    close(fd);

    return BB_SUCCESS();
}

#endif

#if defined(BB_RANDOM_BACKEND_WINDOWS)

static bb_error_t random_bytes_impl(unsigned char *buffer, size_t size)
{
    NTSTATUS status = BCryptGenRandom(
        NULL,
        buffer,
        (ULONG)size,
        BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    if (status != STATUS_SUCCESS)
        return BB_ERROR(BB_ERR_RANDOM_FAILED, "BCryptGenRandom failed");

    return BB_SUCCESS();
}

#elif defined(BB_RANDOM_BACKEND_GETENTROPY)

/* getentropy() rejects requests larger than 256 bytes, so chunk them. */
#define BB_GETENTROPY_MAX 256

static bb_error_t random_bytes_impl(unsigned char *buffer, size_t size)
{
    size_t filled = 0;

    while (filled < size)
    {
        size_t chunk = size - filled;

        if (chunk > BB_GETENTROPY_MAX)
            chunk = BB_GETENTROPY_MAX;

        if (getentropy(buffer + filled, chunk) != 0)
            return random_bytes_urandom(buffer, size);

        filled += chunk;
    }

    return BB_SUCCESS();
}

#elif defined(BB_RANDOM_BACKEND_LINUX)

static bb_error_t random_bytes_impl(unsigned char *buffer, size_t size)
{
    size_t filled = 0;

    while (filled < size)
    {
        ssize_t n = getrandom(buffer + filled, size - filled, 0);

        if (n < 0)
        {
            /* Older kernels (< 3.17) don't implement getrandom() at all. */
            if (errno == ENOSYS)
                return random_bytes_urandom(buffer, size);

            if (errno == EINTR)
                continue;

            return BB_ERROR(BB_ERR_RANDOM_FAILED, "getrandom() failed");
        }

        filled += (size_t)n;
    }

    return BB_SUCCESS();
}

#else /* BB_RANDOM_BACKEND_URANDOM */

static bb_error_t random_bytes_impl(unsigned char *buffer, size_t size)
{
    return random_bytes_urandom(buffer, size);
}

#endif

bb_error_t _bb_random_bytes(void *buffer, size_t size)
{
    if (!buffer)
        return BB_ERROR(BB_ERR_NULL, "null buffer");

    if (size == 0)
        return BB_SUCCESS();

    return random_bytes_impl((unsigned char *)buffer, size);
}

bb_error_t _bb_random_hex(char *buffer, size_t hex_size)
{
    static const char *hex_digits = "0123456789abcdef";

    if (!buffer)
        return BB_ERROR(BB_ERR_NULL, "null buffer");

    if (hex_size == 0)
        return BB_ERROR(BB_ERR_RANDOM_FAILED, "hex buffer has no room for a terminator");

    size_t chars_needed = hex_size - 1;
    size_t pos = 0;
    unsigned char chunk[64];

    while (pos < chars_needed)
    {
        size_t remaining_chars = chars_needed - pos;
        size_t bytes_this_round = (remaining_chars + 1) / 2;

        if (bytes_this_round > sizeof(chunk))
            bytes_this_round = sizeof(chunk);

        bb_error_t err = _bb_random_bytes(chunk, bytes_this_round);

        if (BB_FAILED(err))
            return err;

        for (size_t i = 0; i < bytes_this_round && pos < chars_needed; i++)
        {
            buffer[pos++] = hex_digits[(chunk[i] >> 4) & 0x0F];

            if (pos < chars_needed)
                buffer[pos++] = hex_digits[chunk[i] & 0x0F];
        }
    }

    buffer[chars_needed] = '\0';

    return BB_SUCCESS();
}
