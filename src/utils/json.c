#include "utils/json.h"
#include "error/assert.h"

#include <stdlib.h>
#include <string.h>

void init_json(json_node_t *json, json_node_type type)
{
    json->type = type;
    json->size = 0;
    json->alloc_size = 0;
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

void push_json_array(json_node_t *json_array, json_node_t *element)
{
    BB_ASSERT(json_array->type == array, "Invalid JSON type.");
    if (json_array->alloc_size == 0)
    {
        json_array->alloc_size = 1;
        json_array->value.array = (json_node_t **)malloc(json_array->alloc_size);
    }
    else if (json_array->alloc_size == json_array->size)
    {
        json_array->alloc_size *= 2;
        json_array->value.array = realloc(json_array->value.array, json_array->alloc_size * sizeof(*json_array->value.array));
    }
    json_array->value.array[json_array->size] = element; // Deep Copy?
    json_array->size++;
}

json_node_t *get_json_array_index(json_node_t *json_array, unsigned int index)
{
    BB_ASSERT(json_array->type == array, "Invalid JSON type.");
    BB_ASSERT(json_array->size > index, "Index larger than array size");
    return json_array->value.array[index];
}

// JSON Object: Implemented as key/val array for now
// Will be updated as hash table

static int get_json_object_index(json_node_t *json_object, const char *key)
{
    BB_ASSERT(json_object->type == object, "Invalid JSON type.");
    for (int i = 0; i < json_object->size; i++)
        if (strcmp(json_object->key[i], key) == 0)
            return i;
    return -1;
}

void set_json_object_value(json_node_t *json_object, const char *key, json_node_t *value)
{
    BB_ASSERT(json_object->type == object, "Invalid JSON type.");
    if (!json_object || !value) return;
    int index = get_json_object_index(json_object, key);
    if (0 && index >= 0)
    {
        destroy_json(json_object->value.array[index]);
    }
    else
    {
        if (json_object->alloc_size == 0)
        {
            json_object->alloc_size = 1;
            json_object->key = (char **)malloc(json_object->alloc_size);
            json_object->value.array = (json_node_t **)malloc(json_object->alloc_size);
        }
        else if (json_object->size == json_object->alloc_size)
        {
            json_object->alloc_size *= 2;
            json_object->key = realloc(json_object->key, json_object->alloc_size * sizeof(*json_object->key));
            json_object->value.array = realloc(json_object->value.array, json_object->alloc_size * sizeof(*json_object->value.array));
        }
        index = json_object->size;
        json_object->size++;
    }
    json_object->key[index] = strdup(key);
    json_object->value.array[index] = value; // Deep Copy?
}

json_node_t *get_json_object_value(json_node_t *json_object, const char *key)
{
    BB_ASSERT(json_object->type == object, "Invalid JSON type.");
    int index = get_json_object_index(json_object, key);
    if (index > -1)
        return json_object->value.array[index];
    return NULL;
}

int serialize_null_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == null, "Invalid JSON type.");
    return snprintf(buffer, sizeof(buffer), "null");
}

int serialize_bool_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == boolean, "Invalid JSON type.");
    return snprintf(buffer, sizeof(buffer), "%s", json->value.bool_val ? "true" : "false");
}

int serialize_int_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == integer, "Invalid JSON type.");
    return snprintf(buffer, sizeof(buffer), "%d", json->value.int_val);
}

int serialize_real_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == real, "Invalid JSON type.");
    return snprintf(buffer, sizeof(buffer), "%f", json->value.real_val);
}

int serialize_text_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == text, "Invalid JSON type.");
    return snprintf(buffer, sizeof(buffer), "\"%s\"", json->value.text_val);
}

int serialize_array_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == array, "Invalid JSON type.");
    return 0;
}

int serialize_object_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == object, "Invalid JSON type.");
    return 0;
}

int serialize_json(json_node_t *json, char *buffer)
{
    if (!json) return -1;
    switch (json->type)
    {
        case null:
            return serialize_null_json(json, buffer);
            break;
        case boolean:
            return serialize_bool_json(json, buffer);
            break;
        case integer:
            return serialize_int_json(json, buffer);
            break;
        case real:
            return serialize_real_json(json, buffer);
            break;
        case text:
            return serialize_text_json(json, buffer);
            break;
        case array:
            return serialize_array_json(json, buffer);
            break;
        case object:
            return serialize_object_json(json, buffer);
            break;
    }
}

