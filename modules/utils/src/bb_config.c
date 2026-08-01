#include "blue-bird/utils/bb_config.h"
#include "blue-bird/utils/platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
    {
        s++;
    }

    if (*s == '\0')
    {
        return s;
    }

    char *end = s + strlen(s) - 1;

    while (end > s && isspace((unsigned char)*end))
    {
        *end-- = '\0';
    }

    return s;
}

static void strip_quotes(char *s)
{
    size_t len = strlen(s);

    if (len >= 2)
    {
        if ((s[0] == '"' && s[len - 1] == '"') || (s[0] == '\'' && s[len - 1] == '\''))
        {
            memmove(s, s + 1, len - 2);
            s[len - 2] = '\0';
        }
    }
}

static bb_json_t *parse_value(const char *value)
{
    if (strcasecmp(value, "true") == 0)
    {
        return bb_json_new_bool(true);
    }

    if (strcasecmp(value, "false") == 0)
    {
        return bb_json_new_bool(false);
    }

    if (strcasecmp(value, "null") == 0)
    {
        return bb_json_new_null();
    }

    char *end;

    long iv = strtol(value, &end, 10);

    // Add 'end != value' to prevent empty strings from parsing as 0
    if (*end == '\0' && end != value)
    {
        return bb_json_new_int((int)iv);
    }

    float fv = strtof(value, &end);

    if (*end == '\0' && end != value)
    {
        return bb_json_new_real(fv);
    }

    return bb_json_new_text(value);
}

bb_error_t bb_config_load_env(bb_json_t *config, const char *path)
{
    if (!config)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL config.");
    }

    if (!path)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL path.");
    }

    if (bb_json_get_type(config) != BB_JSON_OBJECT)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Config must be a JSON object.");
    }

    FILE *fp = fopen(path, "r");

    if (!fp)
    {
        return BB_ERROR(BB_ERR_IO, "Failed to open file.");
    }

    char line[1024];

    while (fgets(line, sizeof(line), fp))
    {
        char *p = trim(line);

        if (*p == '\0')
        {
            continue;
        }

        if (*p == '#')
        {
            continue;
        }

        if (strncmp(p, "export ", 7) == 0)
        {
            p += 7;
        }

        char *eq = strchr(p, '=');

        if (!eq)
        {
            continue;
        }

        *eq = '\0';

        char *key = trim(p);
        char *value = trim(eq + 1);

        value[strcspn(value, "\r\n")] = '\0';

        strip_quotes(value);

        bb_json_t *node = parse_value(value);

        if (!node)
        {
            fclose(fp);
            return BB_ERROR(BB_ERR_ALLOC, "Allocation failed.");
        }

        bb_error_t err = bb_json_object_set_value(config, key, node);

        if (BB_FAILED(err))
        {
            bb_json_destroy(node);
            fclose(fp);
            return err;
        }
    }

    fclose(fp);

    return BB_SUCCESS();
}

bb_error_t bb_config_load_json(bb_json_t *config, const char *path)
{
    if (!config)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL config.");
    }

    if (!path)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL path.");
    }

    if (bb_json_get_type(config) != BB_JSON_OBJECT)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Config must be a JSON object.");
    }

    bb_json_t *tmp = bb_json_load(path);

    if (!tmp)
    {
        return BB_ERROR(BB_ERR_IO, "Failed to load JSON.");
    }

    if (bb_json_get_type(tmp) != BB_JSON_OBJECT)
    {
        bb_json_destroy(tmp);

        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Root JSON value must be an object.");
    }

    bb_error_t err = bb_json_object_merge(config, tmp);

    bb_json_destroy(tmp);

    return err;
}
