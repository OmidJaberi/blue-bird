#include <string.h>

#include "persist/model.h"

#define MAX_MODEL_APIS 16

static const BB_ModelAPI *g_apis[MAX_MODEL_APIS];
static int g_api_count = 0;

int bb_model_register(const BB_ModelAPI *api)
{
    if (!api || !api->name)
        return -1;

    if (g_api_count >= MAX_MODEL_APIS)
        return -1;

    // prevent duplicate registration
    for (int i = 0; i < g_api_count; i++)
    {
        if (strcmp(g_apis[i]->name, api->name) == 0)
            return 0;
    }

    g_apis[g_api_count++] = api;
    return 0;
}

const BB_ModelAPI *bb_model_get(const char *name)
{
    if (!name)
        return NULL;

    for (int i = 0; i < g_api_count; i++)
    {
        if (strcmp(g_apis[i]->name, name) == 0)
            return g_apis[i];
    }

    return NULL;
}