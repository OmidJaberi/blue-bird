#include <stdlib.h>
#include <time.h>

#include "../internal/random.h"

static int seeded = 0;

static void seed_rng(void)
{
    if (!seeded)
    {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }
}

bb_error_t _bb_random_bytes(void *buffer, size_t size)
{
    unsigned char *p = buffer;

    if (!buffer)
        return BB_ERROR(BB_ERR_NULL, "null buffer");

    seed_rng();

    for (size_t i = 0; i < size; i++)
        p[i] = rand() & 0xFF;

    return BB_SUCCESS();
}

bb_error_t _bb_random_hex(char *buffer, size_t hex_size)
{
    static const char *hex = "0123456789abcdef";

    if (!buffer)
        return BB_ERROR(BB_ERR_NULL, "null buffer");

    seed_rng();

    for (size_t i = 0; i < hex_size - 1; i++)
        buffer[i] = hex[rand() % 16];

    buffer[hex_size - 1] = '\0';

    return BB_SUCCESS();
}
