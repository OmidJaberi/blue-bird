#include <stdio.h>
#include <string.h>

#include "../internal/random.h"
#include "../internal/password_backend.h"

static unsigned long long fnv1a(const char *str)
{
    unsigned long long hash = 14695981039346656037ULL;

    while (*str)
    {
        hash ^= (unsigned char)*str++;
        hash *= 1099511628211ULL;
    }

    return hash;
}

bb_error_t _bb_password_backend_hash(const char *password, const char *salt, char *output, size_t output_size)
{
    char buffer[512];

    snprintf(buffer, sizeof(buffer), "%s%s", salt, password);

    unsigned long long h = fnv1a(buffer);

    snprintf(output, output_size, "%016llx", h);

    return BB_SUCCESS();
}

bb_error_t bb_password_hash(const char *password, char *out, size_t out_size)
{
    char salt[33];
    char hash[64];

    if (!password || !out)
        return BB_ERROR(BB_ERR_NULL, "null argument");

    _bb_random_hex(salt, sizeof(salt));

    _bb_password_backend_hash(password, salt, hash, sizeof(hash));

    snprintf(out, out_size, "bb$sha256$%s$%s", salt, hash);

    return BB_SUCCESS();
}

int bb_password_verify(const char *password, const char *stored)
{
    char algo[32];
    char salt[64];
    char hash[128];
    char generated[128];

    if (!password || !stored)
        return 0;

    if (sscanf(stored, "bb$%31[^$]$%63[^$]$%127s", algo, salt, hash) != 3)
        return 0;

    _bb_password_backend_hash(password, salt, generated, sizeof(generated));

    return strcmp(hash, generated) == 0;
}
