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
        case JSON_TEXT:
            if (json->value.text_val)
            {
                free(json->value.text_val);
                json->value.text_val = NULL;
            }
            break;
        case JSON_ARRAY:
            for (int i = 0; i < json->size; i++)
            {
                if (json->value.array[i])
                    destroy_json(json->value.array[i]);
            }
            if (json->value.array)
                free(json->value.array);
            json->value.array = NULL;
            break;
        case JSON_OBJECT:
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
    BB_ASSERT(json->type == JSON_BOOL, "Invalid JSON type.");
    json->value.bool_val = value;
}

void set_json_integer_value(json_node_t *json, int value)
{
    BB_ASSERT(json->type == JSON_INT, "Invalid JSON type.");
    json->value.int_val = value;
}

void set_json_real_value(json_node_t *json, float value)
{
    BB_ASSERT(json->type == JSON_REAL, "Invalid JSON type.");
    json->value.real_val = value;
}

void set_json_text_value(json_node_t *json, const char *value)
{
    BB_ASSERT(json->type == JSON_TEXT, "Invalid JSON type.");
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
    BB_ASSERT(json->type == JSON_BOOL, "Invalid JSON type.");
    return json->value.bool_val;
}

int get_json_integer_value(json_node_t *json)
{
    BB_ASSERT(json->type == JSON_INT, "Invalid JSON type.");
    return json->value.int_val;
}

float get_json_real_value(json_node_t *json)
{
    BB_ASSERT(json->type == JSON_REAL, "Invalid JSON type.");
    return json->value.real_val;
}

char *get_json_text_value(json_node_t *json)
{
    BB_ASSERT(json->type == JSON_TEXT, "Invalid JSON type.");
    return json->value.text_val;
}

void push_json_array(json_node_t *json_array, json_node_t *element)
{
    BB_ASSERT(json_array->type == JSON_ARRAY, "Invalid JSON type.");
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
    BB_ASSERT(json_array->type == JSON_ARRAY, "Invalid JSON type.");
    BB_ASSERT(json_array->size > index, "Index larger than array size");
    return json_array->value.array[index];
}

// JSON Object: Implemented as key/val array for now
// Will be updated as hash table

static int hash_function(const char *str)
{
    int sum = 0;
    for (int i = 0; str[i] != '\0'; i++)
        sum += str[i];
    return sum % HASH_TABLE_SIZE;
}

void set_json_object_value(json_node_t *json_object, const char *key, json_node_t *value)
{
    BB_ASSERT(json_object->type == JSON_OBJECT, "Invalid JSON type.");
    if (!json_object || !value || !key) return;

    int index = hash_function(key);
    hash_table_node_t *node = json_object->value.hash_table[index];

    while (node && strcmp(node->key, key) != 0)
    {
        node = node->next;
    }

    if (node)
    {
        destroy_json(node->value);
        free(node->value);
        node->value = value;
    }
    else
    {
        node = malloc(sizeof(hash_table_node_t));
        node->key = strdup(key);
        node->value = value;
        node->next = json_object->value.hash_table[index];
        json_object->value.hash_table[index] = node;
    }
}

json_node_t *get_json_object_value(json_node_t *json_object, const char *key)
{
    BB_ASSERT(json_object->type == JSON_OBJECT, "Invalid JSON type.");
    int index = hash_function(key);
    hash_table_node_t *node = json_object->value.hash_table[index];
    while (node && strcmp(node->key, key) != 0)
        node = node->next;
    if (node)
        return node->value;
    return NULL;
}

void remove_json_object_value(json_node_t *obj, const char *key_to_remove)
{
    BB_ASSERT(obj->type == JSON_OBJECT, "Invalid JSON type.");
    if (!obj || !key_to_remove)
        return;
    
    int index = hash_function(key_to_remove);
    hash_table_node_t *node = obj->value.hash_table[index];
    hash_table_node_t *par = NULL;
    while (node && strcmp(node->key, key_to_remove) != 0)
    {
        par = node;
        node = node->next;
    }
    if (!node) return;

    if (par)
    {
        par->next = node->next;
    }
    else
    {
        obj->value.hash_table[index] = node->next;
    }
    free(node->key);
    destroy_json(node->value);
    free(node->value);
    free(node);
}

// Serializer
static int serialize_json_to_allocated_buffer(json_node_t *json, char *buffer);

static int serialize_null_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == JSON_NULL, "Invalid JSON type.");
    if (buffer)
        memcpy(buffer, "null\0", 5 * sizeof(char));
    return 4;
}

static int serialize_bool_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == JSON_BOOL, "Invalid JSON type.");
    int size = json->value.bool_val ? 4 : 5;
    if (buffer)
        memcpy(buffer, json->value.bool_val ? "true\0" : "false\0", (size + 1) * sizeof(char));
    return size;
}

static int serialize_int_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == JSON_INT, "Invalid JSON type.");
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
    BB_ASSERT(json->type == JSON_REAL, "Invalid JSON type.");
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
        memcpy(buffer, s, (index + 1) * sizeof(char));
    return index;
}
static int serialize_text_json(json_node_t *json, char *buffer)
{
    BB_ASSERT(json->type == JSON_TEXT, "Invalid JSON type.");
    // Unsafe
    char *s = (char*)malloc((json->size * 6 + 3) * sizeof(char));
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
            case '\b': index += sprintf(s + index, "\\b"); break;
            case '\f': index += sprintf(s + index, "\\f"); break;
            default:
                if (c < 0x20)
                {
                    // control char → \u00XX
                    index += sprintf(s + index, "\\u%04x", c);
                }
                else
                {
                    s[index++] = c;
                }
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
    BB_ASSERT(json->type == JSON_ARRAY, "Invalid JSON type.");
    int len = 0;
    len += buffer ? sprintf(buffer, "[") : 1;
    if (has_indent)
        len += buffer ? sprintf(buffer + len, "\n") : 1;
    for (int i = 0; i < json->size; i++)
    {
        for (int j = 0; has_indent && j < indent + 1; j++)
            len += buffer ? sprintf(buffer + len, "\t") : 1;
        char *child_buffer = buffer ? buffer + len : NULL;
        int serialize_child = has_indent ? serialize_json_with_indent(json->value.array[i], child_buffer, indent + 1) : serialize_json_to_allocated_buffer(json->value.array[i], child_buffer);
        if (serialize_child < 0) return -1;
        len += serialize_child;
        len += buffer ? sprintf(buffer + len, i < json->size - 1 ? ", " : "") : (i < json->size - 1 ? 2 : 0);
        if (has_indent)
            len += buffer ? sprintf(buffer + len, "\n") : 1;
    }
    for (int j = 0; has_indent && j < indent; j++)
        len += buffer ? sprintf(buffer + len, "\t") : 1;
    len += buffer ? sprintf(buffer + len, "]") : 1;
    return len;
}

static int serialize_object_json(json_node_t *json, char *buffer, int indent, bool has_indent)
{
    BB_ASSERT(json->type == JSON_OBJECT, "Invalid JSON type.");
    int len = 0;
    len += buffer ? sprintf(buffer, "{") : 1;
    if (has_indent)
        len += buffer ? sprintf(buffer + len, "\n") : 1;
    for (int i = 0; i < json->size; i++)
    {
        for (int j = 0; has_indent && j < indent + 1; j++)
            len += buffer ? sprintf(buffer + len, "\t") : 1;
        len += buffer ? sprintf(buffer + len, "\"%s\": ", json->key[i]) : strlen(json->key[i]) + 4;
        char *child_buffer = buffer ? buffer + len : NULL;
        int serialize_child = has_indent ? serialize_json_with_indent(json->value.array[i], child_buffer, indent + 1) : serialize_json_to_allocated_buffer(json->value.array[i], child_buffer);
        if (serialize_child < 0) return -1;
        len += serialize_child;
        len += buffer ? sprintf(buffer + len, i < json->size - 1 ? ", " : "") : (i < json->size - 1 ? 2 : 0);
        if (has_indent)
            len += buffer ? sprintf(buffer + len, "\n") : 1;
    }
    for (int j = 0; has_indent && j < indent; j++)
        len += buffer ? sprintf(buffer + len, "\t") : 1;
    len += buffer ? sprintf(buffer + len, "}") : 1;
    return len;
}

static int serialize_json_to_allocated_buffer(json_node_t *json, char *buffer)
{
    if (!json) return -1;
    switch (json->type)
    {
        case JSON_NULL:
            return serialize_null_json(json, buffer);
            break;
        case JSON_BOOL:
            return serialize_bool_json(json, buffer);
            break;
        case JSON_INT:
            return serialize_int_json(json, buffer);
            break;
        case JSON_REAL:
            return serialize_real_json(json, buffer);
            break;
        case JSON_TEXT:
            return serialize_text_json(json, buffer);
            break;
        case JSON_ARRAY:
            return serialize_array_json(json, buffer, 0, false);
            break;
        case JSON_OBJECT:
            return serialize_object_json(json, buffer, 0, false);
            break;
    }
}

static int serialize_json_with_indent(json_node_t *json, char *buffer, int indent)
{
    if (!json) return -1;
    if (json->type == JSON_OBJECT)
    {
        return serialize_object_json(json, buffer, indent, true);
    }
    if (json->type == JSON_ARRAY)
    {
        bool indented = false;
        for (int i = 0; i < json->size; i++)
        {
            json_node_t* child = get_json_array_index(json, i);
            indented = indented || (child->type == JSON_ARRAY || child->type == JSON_OBJECT);
        }
        if (indented)
            return serialize_array_json(json, buffer, indent, true);
    }
    return serialize_json_to_allocated_buffer(json, buffer);
}

int serialize_json(json_node_t *json, char **buffer, int *size)
{
    *size = serialize_json_to_allocated_buffer(json, JSON_NULL);
    if (!buffer)
        return 1;
    *buffer = (char*)malloc(*size * sizeof(char));
    if (!*buffer)
    {
        return 1;
    }
    serialize_json_to_allocated_buffer(json, *buffer);
    return 0;
}

int indented_serialize_json(json_node_t *json, char **buffer, int *size)
{
    *size = serialize_json_with_indent(json, JSON_NULL, 0);
    if (!buffer)
        return 1;
    *buffer = (char*)malloc(*size * sizeof(char));
    if (!*buffer)
    {
        return 1;
    }
    serialize_json_with_indent(json, *buffer, 0);
    return 0;
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
    init_json(json, JSON_NULL);
    if (!is_substr(buffer, "null"))
        return -1;
    json->type = JSON_NULL;
    return 4;
}

static int parse_json_str_true(json_node_t *json, char *buffer)
{
    init_json(json, JSON_BOOL);
    if (!is_substr(buffer, "true"))
        return -1;
    json->type = JSON_BOOL;
    json->value.bool_val = true;
    return 4;
}

static int parse_json_str_false(json_node_t *json, char *buffer)
{
    init_json(json, JSON_BOOL);
    if (!is_substr(buffer, "false"))
        return -1;
    json->type = JSON_BOOL;
    json->value.bool_val = false;
    return 5;
}

static int parse_json_str_number(json_node_t *json, char *buffer)
{
    init_json(json, JSON_INT);
    int index = 0;
    while ((buffer[index] >= '0' && buffer[index] <= '9') || (buffer[index] == '.' && json->type == JSON_INT))
    {
        if (buffer[index] == '.')
            json->type = JSON_REAL;
        index++;
    }
    char num_buff[128];
    memcpy(num_buff, buffer, index);
    num_buff[index] = '\0';
    if (json->type == JSON_INT)
        json->value.int_val = atoi(num_buff);
    else
        json->value.real_val = atof(num_buff);
    return index;
}

static int hex_char_to_int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int parse_json_str_text(json_node_t *json, char *buffer)
{
    BB_ASSERT(buffer[0] == '"', "Invalid str quotation.");
    init_json(json, JSON_TEXT);

    int len = strlen(buffer);
    char *out = malloc(len); // maximum possible length
    if (!out) return -1;

    int i = 1; // input index (skip opening quote)
    int j = 0; // output index

    while (buffer[i] != '"' && buffer[i] != '\0')
    {
        if (buffer[i] == '\\') // escape sequence
        {
            i++;
            switch (buffer[i])
            {
                case '"': out[j++] = '"'; break;
                case '\\': out[j++] = '\\'; break;
                case '/': out[j++] = '/'; break;
                case 'b': out[j++] = '\b'; break;
                case 'f': out[j++] = '\f'; break;
                case 'n': out[j++] = '\n'; break;
                case 'r': out[j++] = '\r'; break;
                case 't': out[j++] = '\t'; break;
                case 'u':
                {
                    // parse \uXXXX
                    if (i + 4 >= len) { free(out); return -1; }
                    int value = 0;
                    for (int k = 1; k <= 4; k++)
                    {
                        int digit = hex_char_to_int(buffer[i + k]);
                        if (digit < 0) { free(out); return -1; }
                        value = (value << 4) | digit;
                    }
                    i += 4;
                    if (value > 0xFF) { free(out); return -1; } // simple one-byte support
                    out[j++] = (char)value;
                    break;
                }
                default:
                    free(out);
                    return -1; // invalid escape
            }
            i++;
        }
        else
        {
            out[j++] = buffer[i++];
        }
    }

    if (buffer[i] != '"') { free(out); return -1; }

    out[j] = '\0';
    json->value.text_val = out;
    json->size = j;

    return i + 1; // number of characters consumed including quotes
}

static bool white_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

static int parse_json_str_array(json_node_t *json, char *buffer)
{
    BB_ASSERT(buffer[0] == '[', "Invalid array start.");
    init_json(json, JSON_ARRAY);
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
    init_json(json, JSON_OBJECT);
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
    
    char *buffer = (char*)malloc((size + 1) * sizeof(char));
    if (!buffer)
    {
        fclose(f);
        return 1;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    destroy_json(json);
    init_json(json, JSON_NULL);

    int res = parse_json_str(json, buffer);
    free(buffer);
    return res < 0 ? -1 : 0;
}

int dump_json(json_node_t *json, const char *path)
{
    FILE *f = fopen(path, "wb");

    if (!f) return 1;

    int size;
    char *buffer;
    if (indented_serialize_json(json, &buffer, &size) != 0)
    {
        if (buffer) free(buffer);
        return 1;
    }
    if (size > 0)
        fwrite(buffer, 1, size, f);

    free(buffer);
    fclose(f);
    return 0;
}
