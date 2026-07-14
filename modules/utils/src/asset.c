#include "blue-bird/utils/asset.h"

#include <stdio.h>
#include <stdlib.h>

bb_error_t bb_asset_text_read_all(const char *path, char **buffer, size_t *length)
{
    FILE *file;
    long size;
    size_t bytes_read;
    char *data;

    if (!path || !buffer)
        return BB_ERROR(BB_ERR_INTERNAL, "Invalid arguement");

    *buffer = NULL;

    file = fopen(path, "rb");
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
        return BB_ERROR(BB_ERR_ALLOC, "Alloc failed");
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
