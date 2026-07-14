#include "blue-bird/utils/asset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)

#include <windows.h>

#define BB_PATH_SEP '\\'
#define BB_MAX_PATH MAX_PATH

#else

#include <limits.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#define BB_PATH_SEP '/'
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define BB_MAX_PATH PATH_MAX

#endif

bb_error_t bb_asset_resolve_path(const char *path, char *resolved, size_t size)
{
    if (!path || !resolved)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid argument");
    }

#if defined(_WIN32)

    if ((strlen(path) > 2 && path[1] == ':') || (path[0] == '\\' || path[0] == '/'))
    {
        strncpy(resolved, path, size - 1);
        resolved[size - 1] = '\0';
        return BB_SUCCESS();
    }

    DWORD len = GetModuleFileNameA(NULL, resolved, (DWORD)size);
    if (len == 0 || len >= size)
    {
        return BB_ERROR(BB_ERR_IO, "Unable to determine executable path");
    }

#else

    if (path[0] == '/')
    {
        strncpy(resolved, path, size - 1);
        resolved[size - 1] = '\0';
        return BB_SUCCESS();
    }

#if defined(__APPLE__)

    uint32_t len = (uint32_t)size;

    if (_NSGetExecutablePath(resolved, &len) != 0)
    {
        return BB_ERROR(BB_ERR_IO, "Unable to determine executable path");
    }

#else

    ssize_t len = readlink("/proc/self/exe", resolved, size - 1);

    if (len < 0)
    {
        return BB_ERROR(BB_ERR_IO, "Unable to determine executable path");
    }

    resolved[len] = '\0';

#endif

#endif

    char *slash = strrchr(resolved, BB_PATH_SEP);
    if (!slash)
    {
        return BB_ERROR(BB_ERR_IO, "Invalid executable path");
    }

    *(slash + 1) = '\0';

    if (strlen(resolved) + strlen(path) + 1 > size)
    {
        return BB_ERROR(BB_ERR_IO, "Resolved asset path too long");
    }

    strcat(resolved, path);

    return BB_SUCCESS();
}

bb_error_t bb_asset_text_read_all(const char *path, char **buffer, size_t *length)
{
    FILE *file;
    long size;
    size_t bytes_read;
    char *data;
    char resolved[BB_MAX_PATH];

    if (!path || !buffer)
    {
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid argument");
    }

    bb_error_t err = bb_asset_resolve_path(path, resolved, sizeof(resolved));
    if (BB_FAILED(err))
    {
        return err;
    }

    *buffer = NULL;

    file = fopen(resolved, "rb");
    if (!file)
        return BB_ERROR(BB_ERR_IO, "File not found");

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return BB_ERROR(BB_ERR_IO, "Unable to read file");
    }

    size = ftell(file);
    if (size < 0)
    {
        fclose(file);
        return BB_ERROR(BB_ERR_IO, "Unable to read file");
    }

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return BB_ERROR(BB_ERR_IO, "Unable to read file");
    }

    data = malloc((size_t)size + 1);
    if (!data)
    {
        fclose(file);
        return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
    }

    bytes_read = fread(data, 1, (size_t)size, file);

    if (bytes_read != (size_t)size)
    {
        free(data);
        fclose(file);
        return BB_ERROR(BB_ERR_IO, "Unable to read file");
    }

    fclose(file);

    data[size] = '\0';

    *buffer = data;

    if (length)
    {
        *length = (size_t)size;
    }

    return BB_SUCCESS();
}
