#include "blue-bird/utils/json.h"
#include "blue-bird/error/assert.h"

#include <stdlib.h>
#include <string.h>

#define BB_JSON_INITIAL_BUCKET_COUNT 8
#define BB_JSON_MAX_LOAD_NUM 3
#define BB_JSON_MAX_LOAD_DEN 4

typedef struct BBHashTableNode {
    char *key;
    struct BBJsonNode *value;
    struct BBHashTableNode *next;
    struct BBHashTableNode *order_prev, *order_next;
} _bb_hash_table_node_t;

struct BBJsonNode {
    bb_json_node_type_t type;
    size_t size;               // For text, array, and object types
    union {
        bool bool_val;
        int int_val;
        float real_val;
        char *text_val;
        struct {
            size_t alloc_size; // allocated size for dynamic array
            struct BBJsonNode **array;
        } dynamic_array;
        struct {
            size_t bucket_count;
            size_t item_count;
            _bb_hash_table_node_t **buckets;
            _bb_hash_table_node_t *order_head;
            _bb_hash_table_node_t *order_tail;
        } object;
    };
};

bb_json_t *bb_json_create(bb_json_node_type_t type)
{
    bb_json_t *json = malloc(sizeof(bb_json_t));
    if (!json)
    {
        return NULL;
    }
    json->type = type;
    json->size = 0;
    switch (type)
    {
        case BB_JSON_OBJECT:
        {
            json->object.bucket_count = BB_JSON_INITIAL_BUCKET_COUNT;
            json->object.item_count = 0;
            json->object.buckets = calloc(json->object.bucket_count, sizeof(*json->object.buckets));
            json->object.order_head = NULL;
            json->object.order_tail = NULL;
            break;
        }
        case BB_JSON_ARRAY:
        {
            json->dynamic_array.alloc_size = 0;
            json->dynamic_array.array = NULL;
            break;
        }
        case BB_JSON_TEXT:
        {
            json->text_val = NULL;
            break;
        }
        default:
            break;
    }
    return json;
}

void bb_json_destroy(bb_json_t *json)
{
    if (!json)
    {
        return;
    }
    switch (json->type)
    {
        case BB_JSON_TEXT:
        {
            if (json->text_val)
            {
                free(json->text_val);
                json->text_val = NULL;
            }
            break;
        }
        case BB_JSON_ARRAY:
        {
            for (size_t i = 0; i < json->size; i++)
            {
                if (json->dynamic_array.array[i])
                {
                    bb_json_destroy(json->dynamic_array.array[i]);
                }
            }
            if (json->dynamic_array.array)
                free(json->dynamic_array.array);
            json->dynamic_array.array = NULL;
            break;
        }
        case BB_JSON_OBJECT:
        {
            _bb_hash_table_node_t *node = json->object.order_head;
            while (node)
            {
                _bb_hash_table_node_t *next = node->order_next;
                free(node->key);
                bb_json_destroy(node->value);
                free(node);
                node = next;
            }
            free(json->object.buckets);
            json->object.buckets = NULL;
            json->object.bucket_count = 0;
            json->object.item_count = 0;
            json->object.order_head = NULL;
            json->object.order_tail = NULL;
            break;
        }
        default:
            break;
    }
    free(json);
}

size_t bb_json_get_size(bb_json_t *json)
{
    if (!json)
    {
        return 0;
    }
    return json->size;
}

bb_json_node_type_t bb_json_get_type(bb_json_t *json)
{
    if (!json)
    {
        return BB_JSON_NOT_INITIALIZED;
    }
    return json->type;
}

bb_error_t bb_json_set_value_bool(bb_json_t *json, bool value)
{
    if (json->type != BB_JSON_BOOL)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Invalid JSON type");
    }
    json->bool_val = value;
    return BB_SUCCESS();
}

bb_error_t bb_json_set_value_integer(bb_json_t *json, int value)
{
    if (json->type != BB_JSON_INT)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Invalid JSON type");
    }
    json->int_val = value;
    return BB_SUCCESS();
}

bb_error_t bb_json_set_value_real(bb_json_t *json, float value)
{
    if (json->type != BB_JSON_REAL)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Invalid JSON type");
    }
    json->real_val = value;
    return BB_SUCCESS();
}

bb_error_t bb_json_set_value_text(bb_json_t *json, const char *value)
{
    if (json->type != BB_JSON_TEXT)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Invalid JSON type");
    }
    if (json->text_val)
    {
        free(json->text_val);
        json->text_val = NULL;
        json->size = 0;
    }
    if (!value)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL string");
    }
    size_t len = strlen(value);
    json->text_val = (char *)malloc(len + 1);
    if (!json->text_val)
    {
        return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
    }
    memcpy(json->text_val, value, len);
    json->text_val[len] = '\0';
    json->size = len;
    return BB_SUCCESS();
}

bool bb_json_get_value_bool(bb_json_t *json)
{
    if (json->type != BB_JSON_BOOL)
    {
        return false;
    }
    return json->bool_val;
}

int bb_json_get_value_integer(bb_json_t *json)
{
    if (json->type != BB_JSON_INT)
    {
        return 0;
    }
    return json->int_val;
}

float bb_json_get_value_real(bb_json_t *json)
{
    if (json->type != BB_JSON_REAL)
    {
        return 0.0;
    }
    return json->real_val;
}

char *bb_json_get_value_text(bb_json_t *json)
{
    if (json->type != BB_JSON_TEXT)
    {
        return NULL;
    }
    return json->text_val;
}

bb_error_t bb_json_array_push(bb_json_t *json_array, bb_json_t *element)
{
    if (!json_array || !element)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL JSON object");
    }
    if (json_array->type != BB_JSON_ARRAY)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Invalid JSON type");
    }
    if (json_array->dynamic_array.alloc_size == 0)
    {
        json_array->dynamic_array.alloc_size = 1;
        json_array->dynamic_array.array = (bb_json_t **)malloc(json_array->dynamic_array.alloc_size * sizeof(*json_array->dynamic_array.array));
        if (!json_array->dynamic_array.array)
        {
            return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
        }
    }
    else if (json_array->dynamic_array.alloc_size == json_array->size)
    {
        bb_json_t **new_arr = realloc(json_array->dynamic_array.array, json_array->dynamic_array.alloc_size * 2 * sizeof(*json_array->dynamic_array.array));
        if (!new_arr)
        {
            return BB_ERROR(BB_ERR_ALLOC, "Allocation failed");
        }
        json_array->dynamic_array.alloc_size *= 2;
        json_array->dynamic_array.array = new_arr;
    }
    json_array->dynamic_array.array[json_array->size] = element;
    json_array->size++;
    return BB_SUCCESS();
}

bb_json_t *bb_json_array_get_index(bb_json_t *json_array, unsigned int index)
{
    if (!json_array || json_array->type != BB_JSON_ARRAY || json_array->size <= index)
    {
        return NULL;
    }
    return json_array->dynamic_array.array[index];
}

bb_error_t bb_json_array_remove_at_index(bb_json_t *json_array, unsigned int index)
{
    if (!json_array)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL JSON object");
    }
    if (json_array->type != BB_JSON_ARRAY)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Invalid JSON type");
    }
    if (json_array->size <= index)
    {
        return BB_ERROR(BB_ERR_JSON_OVERFLOW, "Index larger than array size");
    }
    bb_json_destroy(json_array->dynamic_array.array[index]);
    for (unsigned int i = index; i < json_array->size; i++)
    {
        json_array->dynamic_array.array[i] = (i + 1 < json_array->dynamic_array.alloc_size ? json_array->dynamic_array.array[i + 1] : NULL);
    }
    json_array->size--;
    return BB_SUCCESS();
}

// JSON Object: Implemented as hash table

static size_t hash_function(const char *str, size_t bucket_count)
{
    // djb2 hash
    size_t hash = 5381;
    while (*str)
    {
        hash = ((hash << 5) + hash) + (unsigned char)(*str);
        str++;
    }
    return hash & (bucket_count - 1);
}

static void bb_json_object_resize(bb_json_t *json_object, size_t new_bucket_count)
{
    _bb_hash_table_node_t **new_buckets = calloc(new_bucket_count, sizeof(*new_buckets));
    for (size_t i = 0; i < json_object->object.bucket_count; i++)
    {
        _bb_hash_table_node_t *node = json_object->object.buckets[i];
        while (node)
        {
            _bb_hash_table_node_t *next = node->next;
            size_t index = hash_function(node->key, new_bucket_count);
            node->next = new_buckets[index];
            new_buckets[index] = node;
            node = next;
        }
    }
    free(json_object->object.buckets);
    json_object->object.buckets = new_buckets;
    json_object->object.bucket_count = new_bucket_count;
}

bb_error_t bb_json_object_set_value(bb_json_t *json_object, const char *key, bb_json_t *value)
{
    if (!json_object || !value)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL JSON");
    }
    if (!key)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL key");
    }
    if (json_object->type != BB_JSON_OBJECT)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Invalid JSON type");
    }

    // Resize if load factor > 0.75
    if (json_object->object.item_count * BB_JSON_MAX_LOAD_DEN >= json_object->object.bucket_count * BB_JSON_MAX_LOAD_NUM)
    {
        bb_json_object_resize(json_object, json_object->object.bucket_count * 2);
    }
    size_t index = hash_function(key, json_object->object.bucket_count);
    _bb_hash_table_node_t *node = json_object->object.buckets[index];

    while (node && strcmp(node->key, key) != 0)
    {
        node = node->next;
    }

    // Replace existing value
    if (node)
    {
        bb_json_destroy(node->value);
        node->value = value;
        return BB_SUCCESS();
    }
    // Create new node
    node = malloc(sizeof(*node));

    node->key = strdup(key);
    node->value = value;

    // Insert into bucket chain
    node->next = json_object->object.buckets[index];
    json_object->object.buckets[index] = node;

    // Insert into ordered list
    node->order_prev = json_object->object.order_tail;
    node->order_next = NULL;
    if (json_object->object.order_tail)
    {
        json_object->object.order_tail->order_next = node;
    }
    else
    {
        json_object->object.order_head = node;
    }
    json_object->object.order_tail = node;
    json_object->object.item_count++;
    json_object->size++;
    return BB_SUCCESS();
}

bb_json_t *bb_json_object_get_value(bb_json_t *json_object, const char *key)
{
    if (!json_object || !key || json_object->type != BB_JSON_OBJECT)
    {
        return NULL;
    }
    size_t index = hash_function(key, json_object->object.bucket_count);
    _bb_hash_table_node_t *node = json_object->object.buckets[index];
    while (node && strcmp(node->key, key) != 0)
    {
        node = node->next;
    }
    return node ? node->value : NULL;
}

bb_error_t bb_json_object_remove_key(bb_json_t *obj, const char *key_to_remove)
{
    if (!obj)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL JSON");
    }
    if (!key_to_remove)
    {
        return BB_ERROR(BB_ERR_NULL, "NULL key");
    }
    if (obj->type != BB_JSON_OBJECT)
    {
        return BB_ERROR(BB_ERR_JSON_TYPE_MISMATCH, "Invalid JSON type");
    }
    size_t index = hash_function(key_to_remove, obj->object.bucket_count);
    _bb_hash_table_node_t *node = obj->object.buckets[index];

    _bb_hash_table_node_t *prev = NULL;
    while (node && strcmp(node->key, key_to_remove) != 0)
    {
        prev = node;
        node = node->next;
    }
    if (!node)
    {
        return BB_ERROR(BB_ERR_NOT_FOUND, "Key not found");
    }

    // Remove from bucket chain
    if (prev)
    {
        prev->next = node->next;
    }
    else
    {
        obj->object.buckets[index] = node->next;
    }

    // Remove from ordered list
    if (node->order_prev)
    {
        node->order_prev->order_next = node->order_next;
    }
    else
    {
        obj->object.order_head = node->order_next;
    }

    if (node->order_next)
    {
        node->order_next->order_prev = node->order_prev;
    }
    else
    {
        obj->object.order_tail = node->order_prev;
    }

    free(node->key);
    bb_json_destroy(node->value);
    free(node);
    obj->object.item_count--;
    obj->size--;
    return BB_SUCCESS();
}

// Serializer
static int serialize_json_to_allocated_buffer(bb_json_t *json, char *buffer);

static int serialize_null_json(bb_json_t *json, char *buffer)
{
    BB_ASSERT(json->type == BB_JSON_NULL, "Invalid JSON type.");
    if (buffer)
        memcpy(buffer, "null\0", 5 * sizeof(char));
    return 4;
}

static int serialize_bool_json(bb_json_t *json, char *buffer)
{
    BB_ASSERT(json->type == BB_JSON_BOOL, "Invalid JSON type.");
    int size = json->bool_val ? 4 : 5;
    if (buffer)
        memcpy(buffer, json->bool_val ? "true\0" : "false\0", (size + 1) * sizeof(char));
    return size;
}

static int serialize_int_json(bb_json_t *json, char *buffer)
{
    BB_ASSERT(json->type == BB_JSON_INT, "Invalid JSON type.");
    // Unsafe
    if (buffer)
        return sprintf(buffer, "%d", json->int_val);
    int val = json->int_val, len = 0;
    while (val > 0)
    {
        val /= 10;
        len += 1;
    }
    return len == 0 ? 1 : len;
}

static int serialize_real_json(bb_json_t *json, char *buffer)
{
    BB_ASSERT(json->type == BB_JSON_REAL, "Invalid JSON type.");
    // Unsafe
    char s[128];
    int index = sprintf(s, "%f", json->real_val) - 1;
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

static int serialize_text_json(bb_json_t *json, char *buffer)
{
    BB_ASSERT(json->type == BB_JSON_TEXT, "Invalid JSON type.");
    // Unsafe
    char *s = (char*)malloc((json->size * 6 + 3) * sizeof(char));
    if (!s)
        return -1;
    int index = 0;
    index += sprintf(s + index, "\"");

    for (unsigned int i = 0; i < json->size; i++)
    {
        char c = json->text_val[i];
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

static int serialize_json_with_indent(bb_json_t *json, char *buffer, int indent);

static int serialize_array_json(bb_json_t *json, char *buffer, int indent, bool has_indent)
{
    BB_ASSERT(json->type == BB_JSON_ARRAY, "Invalid JSON type.");
    int len = 0;
    len += buffer ? sprintf(buffer, "[") : 1;
    if (has_indent)
        len += buffer ? sprintf(buffer + len, "\n") : 1;
    for (unsigned int i = 0; i < json->size; i++)
    {
        for (int j = 0; has_indent && j < indent + 1; j++)
            len += buffer ? sprintf(buffer + len, "\t") : 1;
        char *child_buffer = buffer ? buffer + len : NULL;
        int serialize_child = has_indent ? serialize_json_with_indent(json->dynamic_array.array[i], child_buffer, indent + 1) : serialize_json_to_allocated_buffer(json->dynamic_array.array[i], child_buffer);
        if (serialize_child < 0) return -1;
        len += serialize_child;
        len += buffer ? sprintf(buffer + len, (i + 1) < json->size ? ", " : "") : ((i + 1) < json->size ? 2 : 0);
        if (has_indent)
            len += buffer ? sprintf(buffer + len, "\n") : 1;
    }
    for (int j = 0; has_indent && j < indent; j++)
        len += buffer ? sprintf(buffer + len, "\t") : 1;
    len += buffer ? sprintf(buffer + len, "]") : 1;
    return len;
}

static int serialize_object_json(bb_json_t *json, char *buffer, int indent, bool has_indent)
{
    BB_ASSERT(json->type == BB_JSON_OBJECT, "Invalid JSON type.");
    size_t len = 0;
    len += buffer ? sprintf(buffer, "{") : 1;
    bool first = true;
    _bb_hash_table_node_t *node = json->object.order_head;
    while (node != NULL)
    {
        if (!first)
        {
            len += buffer ? sprintf(buffer + len, ", ") : 2;
        }
        if (has_indent) len += buffer ? sprintf(buffer + len, "\n") : 1;
        first = false;
        for (int j = 0; has_indent && j < indent + 1; j++)
        len += buffer ? sprintf(buffer + len, "\t") : 1;
        len += buffer ? (size_t)sprintf(buffer + len, "\"%s\": ", node->key) : strlen(node->key) + 4;
        char *child_buffer = buffer ? buffer + len : NULL;
        int serialize_child = has_indent ? serialize_json_with_indent(node->value, child_buffer, indent + 1) : serialize_json_to_allocated_buffer(node->value, child_buffer);
        if (serialize_child < 0) return -1;
        len += serialize_child;
        node = node->order_next;
    }
    if (has_indent) len += buffer ? sprintf(buffer + len, "\n") : 1;
    for (int j = 0; has_indent && j < indent; j++)
        len += buffer ? sprintf(buffer + len, "\t") : 1;
    len += buffer ? sprintf(buffer + len, "}") : 1;
    return len;
}

static int serialize_json_to_allocated_buffer(bb_json_t *json, char *buffer)
{
    if (!json) return -1;
    switch (json->type)
    {
        case BB_JSON_NULL:
            return serialize_null_json(json, buffer);
            break;
        case BB_JSON_BOOL:
            return serialize_bool_json(json, buffer);
            break;
        case BB_JSON_INT:
            return serialize_int_json(json, buffer);
            break;
        case BB_JSON_REAL:
            return serialize_real_json(json, buffer);
            break;
        case BB_JSON_TEXT:
            return serialize_text_json(json, buffer);
            break;
        case BB_JSON_ARRAY:
            return serialize_array_json(json, buffer, 0, false);
            break;
        case BB_JSON_OBJECT:
            return serialize_object_json(json, buffer, 0, false);
            break;
        default:
            return -1;
            break;
    }
}

static int serialize_json_with_indent(bb_json_t *json, char *buffer, int indent)
{
    if (!json) return -1;
    if (json->type == BB_JSON_OBJECT)
    {
        return serialize_object_json(json, buffer, indent, true);
    }
    if (json->type == BB_JSON_ARRAY)
    {
        bool indented = false;
        for (unsigned int i = 0; i < json->size; i++)
        {
            bb_json_t* child = bb_json_array_get_index(json, i);
            indented = indented || (child->type == BB_JSON_ARRAY || child->type == BB_JSON_OBJECT);
        }
        if (indented)
            return serialize_array_json(json, buffer, indent, true);
    }
    return serialize_json_to_allocated_buffer(json, buffer);
}

int bb_json_serialize(bb_json_t *json, char **buffer, int *size)
{
    *size = serialize_json_to_allocated_buffer(json, NULL);
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

int bb_json_serialize_indented(bb_json_t *json, char **buffer, int *size)
{
    *size = serialize_json_with_indent(json, NULL, 0);
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
    for (size_t i = 0; i < strlen(str); i++)
        if (buffer[i] != str[i])
            return false;
    return true;
}

static int parse_json_str_null(bb_json_t **json, char *buffer)
{
    *json = bb_json_create(BB_JSON_NULL);
    if (!is_substr(buffer, "null"))
        return -1;
    return 4;
}

static int parse_json_str_true(bb_json_t **json, char *buffer)
{
    *json = bb_json_create(BB_JSON_BOOL);
    if (!is_substr(buffer, "true"))
        return -1;
    (*json)->type = BB_JSON_BOOL;
    (*json)->bool_val = true;
    return 4;
}

static int parse_json_str_false(bb_json_t **json, char *buffer)
{
    *json = bb_json_create(BB_JSON_BOOL);
    if (!is_substr(buffer, "false"))
        return -1;
    (*json)->type = BB_JSON_BOOL;
    (*json)->bool_val = false;
    return 5;
}

static int parse_json_str_number(bb_json_t **json, char *buffer)
{
    (*json) = bb_json_create(BB_JSON_INT);
    int index = 0;
    while ((buffer[index] >= '0' && buffer[index] <= '9') || (buffer[index] == '.' && (*json)->type == BB_JSON_INT))
    {
        if (buffer[index] == '.')
            (*json)->type = BB_JSON_REAL;
        index++;
    }
    char num_buff[128];
    memcpy(num_buff, buffer, index);
    num_buff[index] = '\0';
    if ((*json)->type == BB_JSON_INT)
        (*json)->int_val = atoi(num_buff);
    else
        (*json)->real_val = atof(num_buff);
    return index;
}

static int hex_char_to_int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static int parse_json_str_text(bb_json_t **json, char *buffer)
{
    BB_ASSERT(buffer[0] == '"', "Invalid str quotation.");
    (*json) = bb_json_create(BB_JSON_TEXT);

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
    (*json)->text_val = out;
    (*json)->size = j;

    return i + 1; // number of characters consumed including quotes
}

static bool white_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n';
}

static int parse_json_str_partial(bb_json_t **json, char *buffer);

static int parse_json_str_array(bb_json_t **json, char *buffer)
{
    BB_ASSERT(buffer[0] == '[', "Invalid array start.");
    (*json) = bb_json_create(BB_JSON_ARRAY);
    int index = 1;
    while (white_space(buffer[index])) index++;
    while (buffer[index] != '\0')
    {
        if (buffer[index] == ']')
            return index + 1;
        bb_json_t *child;
        int res = parse_json_str_partial(&child, buffer + index);
        if (res < 0) return -1;
        bb_json_array_push(*json, child);
        index += res;
        while (white_space(buffer[index])) index++;
        if (buffer[index] == ',')
        {
            index++;
        }
        else if (buffer[index] != ']')
        {
            return -1;
        }
        while (white_space(buffer[index])) index++;
    }
    return -1;
}

static int parse_and_add_json_object_pair(bb_json_t *object, char *buffer)
{
    BB_ASSERT(object->type == BB_JSON_OBJECT, "Invalid JSON type.");
    if (buffer[0] != '\"')
    {
        return -1;
    }
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
    bb_json_t *value;
    int res = parse_json_str_partial(&value, buffer + index);
    if (res < 0) return -1;

    char key[key_end];
    memcpy(key, buffer + 1, key_end - 1);
    key[key_end - 1] = '\0';
    bb_json_object_set_value(object, key, value);
    return res + index;
}

static int parse_json_str_object(bb_json_t **json, char *buffer)
{
    BB_ASSERT(buffer[0] == '{', "Invalid object start.");
    (*json) = bb_json_create(BB_JSON_OBJECT);
    int index = 1;
    while (white_space(buffer[index])) index++;
    while (buffer[index] != '\0')
    {
        if (buffer[index] == '}')
            return index + 1;
        int res = parse_and_add_json_object_pair(*json, buffer + index);
        if (res < 0) return -1;
        index += res;
        while (white_space(buffer[index])) index++;
        if (buffer[index] == ',')
        {
            index++;
        }
        else if (buffer[index] != '}')
        {
            return -1;
        }
        while (white_space(buffer[index])) index++;
    }
    return -1;
}

static int parse_json_str_partial(bb_json_t **json, char *buffer)
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

bb_json_t *bb_json_parse(char *buffer)
{
    bb_json_t *json = NULL;
    int res = parse_json_str_partial(&json, buffer);
    if ((size_t)res != strlen(buffer))
    {
        bb_json_destroy(json);
        json = NULL;
    }
    return json;
}

static int compare_json_bool(bb_json_t *json_a, bb_json_t *json_b)
{
    BB_ASSERT(json_a->type == BB_JSON_BOOL, "Invalid JSON type.");
    BB_ASSERT(json_b->type == BB_JSON_BOOL, "Invalid JSON type.");
    return json_a->bool_val == json_b->bool_val ? 0 : 1;
}

static int compare_json_int(bb_json_t *json_a, bb_json_t *json_b)
{
    BB_ASSERT(json_a->type == BB_JSON_INT, "Invalid JSON type.");
    BB_ASSERT(json_b->type == BB_JSON_INT, "Invalid JSON type.");
    return json_a->int_val == json_b->int_val ? 0 : 1;
}

static int compare_json_real(bb_json_t *json_a, bb_json_t *json_b)
{
    BB_ASSERT(json_a->type == BB_JSON_REAL, "Invalid JSON type.");
    BB_ASSERT(json_b->type == BB_JSON_REAL, "Invalid JSON type.");
    return json_a->real_val == json_b->real_val ? 0 : 1;
}

static int compare_json_text(bb_json_t *json_a, bb_json_t *json_b)
{
    BB_ASSERT(json_a->type == BB_JSON_TEXT, "Invalid JSON type.");
    BB_ASSERT(json_b->type == BB_JSON_TEXT, "Invalid JSON type.");
    return strcmp(json_a->text_val, json_b->text_val) == 0 ? 0 : 1;
}

static int compare_json_array(bb_json_t *json_a, bb_json_t *json_b)
{
    BB_ASSERT(json_a->type == BB_JSON_ARRAY, "Invalid JSON type.");
    BB_ASSERT(json_b->type == BB_JSON_ARRAY, "Invalid JSON type.");
    if (json_a->size != json_b->size)
    {
        return -1;
    }
    for (unsigned int i = 0; i < json_a->size; i++)
    {
        if (bb_json_compare(json_a->dynamic_array.array[i], json_b->dynamic_array.array[i]) != 0)
        {
            return -1;
        }
    }
    return 0;
}

static int compare_json_object(bb_json_t *json_a, bb_json_t *json_b)
{
    BB_ASSERT(json_a->type == BB_JSON_OBJECT, "Invalid JSON type.");
    BB_ASSERT(json_b->type == BB_JSON_OBJECT, "Invalid JSON type.");
    if (json_a->size != json_b->size)
    {
        return -1;
    }
    for (_bb_hash_table_node_t *node = json_a->object.order_head; node != NULL; node = node->order_next)
    {
        bb_json_t *value_in_b = bb_json_object_get_value(json_b, node->key);
        if (bb_json_compare(node->value, value_in_b) != 0)
        {
            return -1;
        }
    }
    return 0;
}

int bb_json_compare(bb_json_t *json_a, bb_json_t *json_b)
{
    if (!json_a || !json_b)
    {
        return -1;
    }
    if (json_a->type != json_b->type)
    {
        return -1;
    }
    switch (json_a->type)
    {
        case BB_JSON_NULL:
            return 0;
            break;
        case BB_JSON_BOOL:
            return compare_json_bool(json_a, json_b);
            break;
        case BB_JSON_INT:
            return compare_json_int(json_a, json_b);
            break;
        case BB_JSON_REAL:
            return compare_json_real(json_a, json_b);
            break;
        case BB_JSON_TEXT:
            return compare_json_text(json_a, json_b);
            break;
        case BB_JSON_ARRAY:
            return compare_json_array(json_a, json_b);
            break;
        case BB_JSON_OBJECT:
            return compare_json_object(json_a, json_b);
            break;
        default:
            return -1;
            break;
    }
}

bb_json_t *bb_json_load(const char *path)
{
    FILE *f = fopen(path, "rb");
    
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = (char*)malloc((size + 1) * sizeof(char));
    if (!buffer)
    {
        fclose(f);
        return NULL;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    bb_json_t *json = bb_json_parse(buffer);
    free(buffer);
    return json;
}

bb_error_t bb_json_dump(bb_json_t *json, const char *path)
{
    FILE *f = fopen(path, "wb");

    if (!f)
    {
        return BB_ERROR(BB_ERR_IO, "File can't be opened.");
    }

    int size;
    char *buffer;
    if (bb_json_serialize_indented(json, &buffer, &size) != 0)
    {
        if (buffer) free(buffer);
        return BB_ERROR(BB_ERR_INTERNAL, "Failed to serialize.");
    }
    if (size > 0)
        fwrite(buffer, 1, size, f);

    free(buffer);
    fclose(f);
    return BB_SUCCESS();
}
