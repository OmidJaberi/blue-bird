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
            }
            break;
        case array:
            for (int i = 0; i < json->size; i++)
            {
                if (json->value.array[i])
                    destroy_json(json->value.array[i]);
            }
            if (json->value.array)
                free(json->value.array);
            json->value.array = NULL;
            break;
        case object:
            for (int i = 0; i < json->size; i++)
            {
                if (json->value.array[i])
                    destroy_json(json->value.array[i]);
                if (json->key && json->key[i])
                    free(json->key[i]);
            }
            if (json->value.array)
                free(json->value.array);
            if (json->key)
                free(json->key);
            json->value.array = NULL;
            json->key = NULL;
            break;
        default:
            break;
    }
    json->size = 0;
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
    json_array->value.array[json_array->size] = element;
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
    if (index >= 0)
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
    json_object->value.array[index] = value;
}

json_node_t *get_json_object_value(json_node_t *json_object, const char *key)
{
    BB_ASSERT(json_object->type == object, "Invalid JSON type.");
    int index = get_json_object_index(json_object, key);
    if (index > -1)
        return json_object->value.array[index];
    return NULL;
}

static int serialize_null_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == null, "Invalid JSON type.");
    if (buffer)
        memcpy(buffer, "null", 4 * sizeof(char));
    return 4;
}

static int serialize_bool_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == boolean, "Invalid JSON type.");
    int size = json->value.bool_val ? 4 : 5;
    if (buffer)
        memcpy(buffer, json->value.bool_val ? "true" : "false", size * sizeof(char));
    return size;
}

static int serialize_int_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == integer, "Invalid JSON type.");
    // Unsafe
    if (buffer)
        return sprintf(buffer, "%d", json->value.int_val);
    int val = json->value.int_val, len = 0;
    while (val > 0)
    {
        val /= 10;
        len += 1;
    }
    return len == 0 ? 1 : len;
}

static int serialize_real_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == real, "Invalid JSON type.");
    // Unsafe
    char s[128];
    int index = sprintf(s, "%f", json->value.real_val) - 1;
    while (s[index] == '0')
    {
        s[index] = '\0';
        index--;
    }
    index++;
    if (buffer)
        memcpy(buffer, s, index * sizeof(char));
    return index;
}
static int serialize_text_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == text, "Invalid JSON type.");
    // Unsafe
    char *s = (char*)malloc((json->size * 2 + 10) * sizeof(char));
    if (!s)
        return -1;
    int index = 0;
    index += sprintf(s + index, "\"");

    for (int i = 0; i < json->size; i++)
    {
        char c = json->value.text_val[i];
        switch (c)
        {
            case '\\': index += sprintf(s + index, "\\\\"); break;
            case '\"': index += sprintf(s + index, "\\\""); break;
            case '\n': index += sprintf(s + index, "\\n"); break;
            case '\t': index += sprintf(s + index, "\\t"); break;
            case '\r': index += sprintf(s + index, "\\r"); break;
            default:
                index += sprintf(s + index, "%c", c);
        }
    }

    index += sprintf(s + index, "\"");
    s[index] = '\0';
    if (buffer)
        memcpy(buffer, s, (index + 1) * sizeof(char));
    free(s);
    return index;
}

static int serialize_json_with_indent(json_node_t *json, char *buffer, int indent);

static int serialize_array_json(json_node_t *json, char *buffer, int indent, bool has_indent)
{
    BB_ASSERT(json->type == array, "Invalid JSON type.");
    int len = 0;
    len += sprintf(buffer, "[");
    if (has_indent)
        len += sprintf(buffer + len, "\n");
    for (int i = 0; i < json->size; i++)
    {
        for (int j = 0; has_indent && j < indent + 1; j++)
            len += sprintf(buffer + len, "\t");
        int serialize_child = has_indent ? serialize_json_with_indent(json->value.array[i], buffer + len, indent + 1) : serialize_json(json->value.array[i], buffer + len);
        if (serialize_child < 0) return -1;
        len += serialize_child;
        len += sprintf(buffer + len, i < json->size - 1 ? ", " : "");
        if (has_indent)
            len += sprintf(buffer + len, "\n");
    }
    for (int j = 0; has_indent && j < indent; j++)
        len += sprintf(buffer + len, "\t");
    len += sprintf(buffer + len, "]");
    return len;
}

static int serialize_object_json(json_node_t *json, char *buffer, int indent, bool has_indent)
{
    BB_ASSERT(json->type == object, "Invalid JSON type.");
    int len = 0;
    len += sprintf(buffer, "{");
    if (has_indent)
        len += sprintf(buffer + len, "\n");
    for (int i = 0; i < json->size; i++)
    {
        for (int j = 0; has_indent && j < indent + 1; j++)
            len += sprintf(buffer + len, "\t");
        len += sprintf(buffer + len, "\"%s\": ", json->key[i]);
        int serialize_child = has_indent ? serialize_json_with_indent(json->value.array[i], buffer + len, indent + 1) : serialize_json(json->value.array[i], buffer + len);
        if (serialize_child < 0) return -1;
        len += serialize_child;
        len += sprintf(buffer + len, i < json->size - 1 ? ", " : "");
        if (has_indent)
            len += sprintf(buffer + len, "\n");
    }
    for (int j = 0; has_indent && j < indent; j++)
        len += sprintf(buffer + len, "\t");
    len += sprintf(buffer + len, "}");
    return len;
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
            return serialize_array_json(json, buffer, 0, false);
            break;
        case object:
            return serialize_object_json(json, buffer, 0, false);
            break;
    }
}

static int serialize_json_with_indent(json_node_t *json, char *buffer, int indent)
{
    if (!json) return -1;
    if (json->type == object)
    {
        return serialize_object_json(json, buffer, indent, true);
    }
    if (json->type == array)
    {
        bool indented = false;
        for (int i = 0; i < json->size; i++)
        {
            json_node_t* child = get_json_array_index(json, i);
            indented = indented || (child->type == array || child->type == object);
        }
        if (indented)
            return serialize_array_json(json, buffer, indent, true);
    }
    return serialize_json(json, buffer);
}

int pretty_serialize_json(json_node_t *json, char *buffer)
{
    return serialize_json_with_indent(json, buffer, 0);
}

static bool is_substr(char *buffer, const char *str)
{
    for (int i = 0; i < strlen(str); i++)
        if (buffer[i] != str[i])
            return false;
    return true;
}

static int parse_json_str_null(json_node_t *json, char *buffer)
{
    init_json(json, null);
    if (!is_substr(buffer, "null"))
        return -1;
    json->type = null;
    return 4;
}

static int parse_json_str_true(json_node_t *json, char *buffer)
{
    init_json(json, boolean);
    if (!is_substr(buffer, "true"))
        return -1;
    json->type = boolean;
    json->value.bool_val = true;
    return 4;
}

static int parse_json_str_false(json_node_t *json, char *buffer)
{
    init_json(json, boolean);
    if (!is_substr(buffer, "false"))
        return -1;
    json->type = boolean;
    json->value.bool_val = false;
    return 5;
}

static int parse_json_str_number(json_node_t *json, char *buffer)
{
    init_json(json, integer);
    int index = 0;
    while ((buffer[index] >= '0' && buffer[index] <= '9') || (buffer[index] == '.' && json->type == integer))
    {
        if (buffer[index] == '.')
            json->type = real;
        index++;
    }
    char num_buff[128];
    memcpy(num_buff, buffer, index);
    num_buff[index] = '\0';
    if (json->type == integer)
        json->value.int_val = atoi(num_buff);
    else
        json->value.real_val = atof(num_buff);
    return index;
}

static int parse_json_str_text(json_node_t *json, char *buffer)
{
    BB_ASSERT(buffer[0] == '"', "Invalid str quotation.");
    init_json(json, text);
    int index = 1;
    while (buffer[index] != '"' && buffer[index] != '\0')
        index++;
    if (buffer[index] != '"')
        return -1;
    int len = index - 1;
    json->value.text_val = malloc(len + 1);
    if (!json->value.text_val)
        return -1;
    memcpy(json->value.text_val, buffer + 1, len);
    json->value.text_val[len] = '\0';

    json->size = len;
    return index + 1;
}

static bool white_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

static int parse_json_str_array(json_node_t *json, char *buffer)
{
    BB_ASSERT(buffer[0] == '[', "Invalid array start.");
    init_json(json, array);
    int index = 1;
    while (white_space(buffer[index])) index++;
    while (buffer[index] != '\0')
    {
        if (buffer[index] == ']')
            return index + 1;
        json_node_t *child = (json_node_t*)malloc(sizeof(json_node_t));
        int res = parse_json_str(child, buffer + index);
        if (res < 0) return -1;
        push_json_array(json, child);
        index += res;
        while (white_space(buffer[index])) index++;
        if (buffer[index] == ',') index++;
        while (white_space(buffer[index])) index++;
    }
    return -1;
}

static int parse_and_add_json_object_pair(json_node_t *object, char *buffer)
{
    BB_ASSERT(buffer[0] == '\"', "Invalid key string start.");
    int index = 1;
    while (buffer[index] != '\"' && buffer[index] != '\t')
        index++;
    if (buffer[index] != '\"')
        return -1;
    int key_end = index;
    index++;
    while (white_space(buffer[index])) index++;
    if (buffer[index] != ':')
        return -1;
    index++;
    while (white_space(buffer[index])) index++;
    json_node_t *value = (json_node_t*)malloc(sizeof(json_node_t));
    int res = parse_json_str(value, buffer + index);
    if (res < 0) return -1;

    char* key = (char *)malloc(key_end - 1);
    if (!key) return -1; // malloc failed
    memcpy(key, buffer + 1, key_end - 1);
    key[key_end - 1] = '\0';
    set_json_object_value(object, key, value);
    free(key);
    return res + index;
}

static int parse_json_str_object(json_node_t *json, char *buffer)
{
    BB_ASSERT(buffer[0] == '{', "Invalid object start.");
    init_json(json, object);
    int index = 1;
    while (white_space(buffer[index])) index++;
    while (buffer[index] != '\0')
    {
        if (buffer[index] == '}')
            return index + 1;
        int res = parse_and_add_json_object_pair(json, buffer + index);
        if (res < 0) return -1;
        index += res;
        while (white_space(buffer[index])) index++;
        if (buffer[index] == ',') index++;
        while (white_space(buffer[index])) index++;
    }
    return -1;
}

int parse_json_str(json_node_t *json, char *buffer)
{
    int index = 0;
    while (white_space(buffer[index])) index++;
    switch (buffer[index])
    {
        case '{':
            return parse_json_str_object(json, buffer);
            break;
        case '[':
            return parse_json_str_array(json, buffer);
            break;
        case 'n':
            return parse_json_str_null(json, buffer);
            break;
        case 't':
            return parse_json_str_true(json, buffer);
            break;
        case 'f':
            return parse_json_str_false(json, buffer);
            break;
        case '\"':
            return parse_json_str_text(json, buffer);
            break;
        default:
            if (buffer[index] >= '0' && buffer[index] <= '9')
                return parse_json_str_number(json, buffer);
            return -1;
            break;
    }
}

int load_json(json_node_t *json, const char *path)
{
    FILE *f = fopen(path, "rb");
    
    if (!f) return 1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = (char*)malloc(size * sizeof(char));
    if (!buffer)
    {
        fclose(f);
        return 1;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    destroy_json(json);
    init_json(json, null);

    int res = parse_json_str(json, buffer);
    free(buffer);
    return res < 0 ? -1 : 0;
}

int dump_json(json_node_t *json, const char *path)
{
    FILE *f = fopen(path, "wb");

    if (!f) return 1;

    char *buffer = (char*)malloc(100 * 1000 * sizeof(char)); // Dynamic size?
    int size = pretty_serialize_json(json, buffer);

    if (size > 0)
        fwrite(buffer, 1, size, f);

    free(buffer);
    fclose(f);
    return 0;
}
