#include "utils/json.h"
#include "error/assert.h"

#include <stdlib.h>
#include <string.h>

void init_json(json_node_t *json, json_node_type type)
{
    json->type = type;
    json->value.text_val = NULL;
}

void destroy_json(json_node_t *json)
{
    switch (json->type) {
        case text:
            if (json->value.text_val)
            {
                free(json->value.text_val);
                json->value.text_val = NULL;
                json->size = 0;
            }
            break;
        case array:
            // destroy children
            break;
        case object:
            // destroy children
            break;
        default:
            break;
    }
}

json_node_type get_json_type(json_node_t *json)
{
    return json->type;
}

void set_json_bool_value(json_node_t *json, bool value)
{
    BB_ASSERT(json->type == boolean, "Invalid JSON type.");
    json->value.bool_val = value;
}

void set_json_integer_value(json_node_t *json, int value)
{
    BB_ASSERT(json->type == integer, "Invalid JSON type.");
    json->value.int_val = value;
}

void set_json_real_value(json_node_t *json, float value)
{
    BB_ASSERT(json->type == real, "Invalid JSON type.");
    json->value.real_val = value;
}

void set_json_text_value(json_node_t *json, const char *value)
{
    BB_ASSERT(json->type == text, "Invalid JSON type.");
    if (json->value.text_val)
    {
        free(json->value.text_val);
        json->value.text_val = NULL;
        json->size = 0;
    }
    if (!value)
        return;
	size_t len = strlen(value);
	json->value.text_val = (char *)malloc(len + 1);
    if (!json->value.text_val) return; // malloc failed
    memcpy(json->value.text_val, value, len);
    json->value.text_val[len] = '\0';
    json->size = len;
}

bool get_json_bool_value(json_node_t *json)
{
    BB_ASSERT(json->type == boolean, "Invalid JSON type.");
    return json->value.bool_val;
}

int get_json_integer_value(json_node_t *json)
{
    BB_ASSERT(json->type == integer, "Invalid JSON type.");
    return json->value.int_val;
}

float get_json_real_value(json_node_t *json)
{
    BB_ASSERT(json->type == real, "Invalid JSON type.");
    return json->value.real_val;
}

char *get_json_text_value(json_node_t *json)
{
    BB_ASSERT(json->type == text, "Invalid JSON type.");
    return json->value.text_val;
}

