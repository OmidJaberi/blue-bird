#include "utils/json.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void test_json_text()
{
    printf("\tTesting JSON text...\n");
    json_node_t json;
    init_json(&json, JSON_TEXT);
    set_json_text_value(&json, "Hello there!");
    assert(json.size == 12);
    assert(strcmp(get_json_text_value(&json), "Hello there!") == 0);
    destroy_json(&json);
}

void test_json_array()
{
    printf("\tTesting JSON array...\n");
    char *vals[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR"};
    json_node_t arr;
    init_json(&arr, JSON_ARRAY);
    for (int i = 0; i < 5; i++)
    {
        json_node_t *element = malloc(sizeof(json_node_t));
        init_json(element, JSON_TEXT);
        set_json_text_value(element, vals[i]);
        push_json_array(&arr, element);
    }
    assert(arr.size == 5);
    for (int i = 0; i < arr.size; i++)
    {
        assert(strcmp(get_json_text_value(get_json_array_index(&arr, i)), vals[i]) == 0);
    }
    destroy_json(&arr);
}

void test_json_object()
{
    printf("\tTesting JSON object...\n");
    char *keys[] = {"one", "two", "three", "four", "five"};
    char *vals[] = {"ichi", "nii", "san", "yon", "go"};
    json_node_t obj;
    init_json(&obj, JSON_OBJECT);
    for (int i = 0; i < 5; i++)
    {
        json_node_t *value = malloc(sizeof(json_node_t));
        init_json(value, JSON_TEXT);
        set_json_text_value(value, vals[i]);
        set_json_object_value(&obj, keys[i], value);
    }
    for (int i = 0; i < obj.size; i++)
    {
        json_node_t *res = get_json_object_value(&obj, keys[i]);
        assert(strcmp(get_json_text_value(res), vals[i]) == 0);
    }
    destroy_json(&obj);
}

void test_object_key_overwrite()
{
    printf("\tTesting JSON object key overwrite...\n");
    json_node_t obj;
    init_json(&obj, JSON_OBJECT);

    json_node_t *v1 = malloc(sizeof(json_node_t));
    init_json(v1, JSON_INT);
    set_json_integer_value(v1, 1);
    set_json_object_value(&obj, "a", v1);

    json_node_t *v2 = malloc(sizeof(json_node_t));
    init_json(v2, JSON_INT);
    set_json_integer_value(v2, 2);
    set_json_object_value(&obj, "a", v2);

    json_node_t *res = get_json_object_value(&obj, "a");
    assert(get_json_integer_value(res) == 2);

    destroy_json(&obj);
}

void test_object_key_deletion()
{
    printf("\tTesting JSON object key deletion...\n");
    json_node_t obj;
    init_json(&obj, JSON_OBJECT);

    json_node_t *v1 = malloc(sizeof(json_node_t));
    init_json(v1, JSON_INT);
    set_json_integer_value(v1, 1);
    set_json_object_value(&obj, "a", v1);

    remove_json_object_value(&obj, "a");

    json_node_t *res = get_json_object_value(&obj, "a");
    assert(res == NULL);

    destroy_json(&obj);
}

void test_serialize_text_json()
{
    printf("\tTesting serializing JSON text...\n");
    json_node_t json;
    init_json(&json, JSON_TEXT);
    set_json_text_value(&json, "123456");
    char *buffer;
    int size;
    serialize_json(&json, &buffer, &size);
    assert(strcmp(buffer, "\"123456\"") == 0);
    free(buffer);
    destroy_json(&json);
}

void test_serialize_text_json_with_escape_characters()
{
    printf("\tTesting serializing JSON text with escape characters...\n");
    json_node_t json;
    init_json(&json, JSON_TEXT);
    set_json_text_value(&json, "sample text:\t12\\34\nanother line.");
    char *buffer;
    int size;
    serialize_json(&json, &buffer, &size);
    assert(strcmp(buffer, "\"sample text:\\t12\\\\34\\nanother line.\"") == 0);
    free(buffer);
    destroy_json(&json);
}

void test_serialize_array_json()
{
    printf("\tTesting serializing JSON array...\n");
    char *vals[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR"};
    json_node_t arr;
    init_json(&arr, JSON_ARRAY);
    for (int i = 0; i < 5; i++)
    {
        json_node_t *child = (json_node_t*)malloc(sizeof(json_node_t));
        init_json(child, JSON_TEXT);
        set_json_text_value(child, vals[i]);
        push_json_array(&arr, child);
    }
    char *buffer;
    int size;
    serialize_json(&arr, &buffer, &size);
    assert(strcmp(buffer, "[\"ZERO\", \"ONE\", \"TWO\", \"THREE\", \"FOUR\"]") == 0);
    free(buffer);
    destroy_json(&arr);
}

void test_serialize_object_json()
{
    printf("\tTesting serializing JSON object...\n");
    char *keys[] = {"one", "two", "three", "four", "five"};
    char *vals[] = {"ichi", "nii", "san", "yon", "go"};
    json_node_t obj;
    init_json(&obj, JSON_OBJECT);
    for (int i = 0; i < 4; i++)
    {
        json_node_t *value = (json_node_t*)malloc(sizeof(json_node_t));
        init_json(value, JSON_TEXT);
        set_json_text_value(value, vals[i]);
        set_json_object_value(&obj, keys[i], value);
    }
    char *buffer;
    int size;
    serialize_json(&obj, &buffer, &size);
    assert(strcmp(buffer, "{\"one\": \"ichi\", \"two\": \"nii\", \"three\": \"san\", \"four\": \"yon\"}") == 0);
    free(buffer);
    destroy_json(&obj);
}

void test_serialize_large_json()
{
    printf("\tTesting serializing large JSON object...\n");
    int n = 100;
    json_node_t json;
    init_json(&json, JSON_OBJECT);
    for (int i = 0; i < n; i++)
    {
        char key[4];
        sprintf(key, "%d", i);
        json_node_t *value = (json_node_t*)malloc(sizeof(json_node_t));
        init_json(value, JSON_ARRAY);
        for (int j = 0; j < i; j++)
        {
            json_node_t *child = (json_node_t*)malloc(sizeof(json_node_t));
            init_json(child, JSON_INT);
            set_json_integer_value(child, j);
            push_json_array(value, child);
        }
        set_json_object_value(&json, key, value);
    }
    
    char *large_buffer = (char *)malloc(sizeof(char) * 20000);
    int index = sprintf(large_buffer, "{");
    for (int i = 0; i < n; i++)
    {
        index += sprintf(large_buffer + index, "\"%d\": ", i);
        index += sprintf(large_buffer + index, "[");
        for (int j = 0; j < i; j++)
            index += sprintf(large_buffer + index, "%d%s", j, j < i - 1 ? ", " : "");
        index += sprintf(large_buffer + index, "]%s", i < n - 1 ? ", " : "");
    }
    index += sprintf(large_buffer + index, "}");

    char *serialize_buffer;
    int size;
    serialize_json(&json, &serialize_buffer, &size);

    assert(strcmp(large_buffer, serialize_buffer) == 0);
    free(serialize_buffer);
    free(large_buffer);
    destroy_json(&json);
}

void test_parse_and_serialize_json()
{
    printf("\tTesting JSON parsing...\n");
    char *s = "[\"one\", \"two\", {\"some thing\": null, \"other thing\": false, \"and the other thing\": [1, 2, 3, 11.47]}, null, [\"first\", 15, true, false]]";
    json_node_t json;
    parse_json_str(&json, s);

    char *buffer;
    int size;
    serialize_json(&json, &buffer, &size);
    assert(strcmp(buffer, s) == 0);
    free(buffer);
    destroy_json(&json);
}

void test_parse_large_json()
{
    printf("\tTesting large JSON parsing...\n");
    int n = 100;
    char *large_buffer = (char *)malloc(sizeof(char) * 20000);
    int index = sprintf(large_buffer, "{");
    for (int i = 0; i < n; i++)
    {
        index += sprintf(large_buffer + index, "\"%d\": ", i);
        index += sprintf(large_buffer + index, "[");
        for (int j = 0; j < i; j++)
            index += sprintf(large_buffer + index, "%d%s", j, j < i - 1 ? ", " : "");
        index += sprintf(large_buffer + index, "]%s", i < n - 1 ? ", " : "");
    }
    index += sprintf(large_buffer + index, "}");
    json_node_t json;
    parse_json_str(&json, large_buffer);
    assert(json.size == n);
    for (int i = 0; i < n; i++)
    {
        char key[4];
        sprintf(key, "%d", i);
        json_node_t *child = get_json_object_value(&json, key);
        assert(child);
        assert(child->type == JSON_ARRAY);
        assert(child->size == i);
        for (int j = 0; j < i; j++)
        {
            json_node_t *sub_child = get_json_array_index(child, j);
            assert(sub_child->type == JSON_INT);
            assert(get_json_integer_value(sub_child) == j);
        }
    }
    destroy_json(&json);
}

void test_parse_text_with_escapes()
{
    printf("\tTesting parse JSON text with escapes...\n");

    // JSON source text (escaped, not raw C escapes)
    char *s = "\"hello\\nworld\\t\\u0001\"";

    json_node_t json;
    int res = parse_json_str(&json, s);
    assert(res > 0);

    assert(json.type == JSON_TEXT);

    // "hello\nworld\t\x01"
    // indexes: 0123456789012
    assert(json.size == 13);

    assert(json.value.text_val[5] == '\n');
    assert(json.value.text_val[11] == '\t');
    assert((unsigned char)json.value.text_val[12] == 0x01);

    char *buf;
    int size;
    serialize_json(&json, &buf, &size);
    assert(strcmp(buf, "\"hello\\nworld\\t\\u0001\"") == 0);
    free(buf);

    destroy_json(&json);
}

void test_serialize_json_size()
{
    printf("\tTesting serialized JSON size...\n");
    char *s = "[\"one\", \"two\", \"escape\tcharacter\", {\"some thing\": null, \"other thing\": false, \"and the other thing\": [1, 2, 3, 11.47]}, null, [\"first\", 15, true, false]]";
    json_node_t json;
    parse_json_str(&json, s);

    char *buffer;
    int size, null_size, str_size;
    serialize_json(&json, &buffer, &size);
    serialize_json(&json, NULL, &null_size);
    str_size = strlen(buffer);
    assert(size == str_size);
    assert(null_size == str_size);
    free(buffer);
    destroy_json(&json);
}

// Broken JSON Parsing:
void test_incomplete_text_json()
{
    printf("\tTesting incomplete text JSON parsing...\n");
    json_node_t json;
    int res = parse_json_str(&json, "\"text with no ending quotation mark.");
    assert(res == -1);
    destroy_json(&json);
}

void test_incomplete_array_json()
{
    printf("\tTesting incomplete array JSON parsing...\n");
    json_node_t json;
    int res = parse_json_str(&json, "[1, 2, 3");
    assert(res == -1);
    destroy_json(&json);
}

void test_multiple_comma_array_json()
{
    printf("\tTesting multiple comma array JSON parsing...\n");
    json_node_t json;
    int res = parse_json_str(&json, "[1, 2, , 3]");
    assert(res == -1);
    destroy_json(&json);
}

void test_dump_and_load_json()
{
    printf("\tTesting JSON file load and dump...\n");
    json_node_t json_1, json_2;
    init_json(&json_1, JSON_ARRAY);
    for (int i = 0; i < 10; i++)
    {
        json_node_t *child = (json_node_t*)malloc(sizeof(json_node_t));
        init_json(child, JSON_INT);
        set_json_integer_value(child, i);
        push_json_array(&json_1, child);
    }
    dump_json(&json_1, "test_file.json");
    load_json(&json_2, "test_file.json");
    char *buf;
    int size;
    serialize_json(&json_2, &buf, &size);
    
    assert(json_1.type == json_2.type);
    assert(json_1.size == json_2.size);
    for (int i = 0; i < json_1.size; i++)
        assert(get_json_integer_value(get_json_array_index(&json_1, i)) == get_json_integer_value(get_json_array_index(&json_2, i)));
    
    destroy_json(&json_1);
    destroy_json(&json_2);
}

void test_json_dsl_macros()
{
    printf("\tTesting JSON DSL Macros...\n");
    json_node_t *doc = JSON(
        OBJ(
            KEY("name", TEXT("Alice")),
            KEY("age", INT(30)),
            KEY("admin", BOOL(false)),
            KEY("scores", ARR(INT(10), INT(20), INT(30))),
            KEY("misc", NULLV()),
            KEY("nested",
                OBJ(
                    KEY("pi", REAL(3.14)),
                    KEY("ok", BOOL(true))
                )
            )
        )
    );
    char *buf;
    int size;
    serialize_json(doc, &buf, &size);
    assert(strcmp(buf, "{\"name\": \"Alice\", \"age\": 30, \"admin\": false, \"scores\": [10, 20, 30], \"misc\": null, \"nested\": {\"pi\": 3.14, \"ok\": true}}") == 0);
    destroy_json(doc);
    free(doc);
}

int main()
{
    printf("Running JSON tests...\n");
    test_json_text();
    test_json_array();
    test_json_object();
    test_object_key_overwrite();
    test_object_key_deletion();

    test_serialize_text_json();
    test_serialize_array_json();
    test_serialize_object_json();
    test_serialize_large_json();

    test_parse_and_serialize_json();
    test_parse_large_json();
    test_parse_text_with_escapes();
    test_serialize_json_size();

    test_incomplete_text_json();
    test_incomplete_array_json();
    test_multiple_comma_array_json();

    test_dump_and_load_json();

    test_json_dsl_macros();
    printf("All tests passed.\n");
    return 0;
}
