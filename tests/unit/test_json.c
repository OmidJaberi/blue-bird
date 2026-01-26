#include "utils/json.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void test_json_text()
{
    printf("\tTesting JSON text...\n");
    json_node_t json;
    init_json(&json, text);
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
    init_json(&arr, array);
    json_node_t element[5];
    for (int i = 0; i < 5; i++)
    {
        init_json(element + i, text);
        set_json_text_value(element + i, vals[i]);
        push_json_array(&arr, element + i);
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
    init_json(&obj, object);
    json_node_t value[5];
    for (int i = 0; i < 4; i++)
    {
        init_json(value + i, text);
        set_json_text_value(value + i, vals[i]);
        set_json_object_value(&obj, keys[i], value + i);
    }
    for (int i = 0; i < obj.size; i++)
    {
        json_node_t *res = get_json_object_value(&obj, keys[i]);
        assert(strcmp(get_json_text_value(res), vals[i]) == 0);
    }
    destroy_json(&obj);
}

void test_serialize_text_json()
{
    printf("\tTesting serializing JSON text...\n");
    json_node_t json;
    init_json(&json, text);
    set_json_text_value(&json, "123456");
    char buffer[128];
    serialize_json(&json, buffer);
    assert(strcmp(buffer, "\"123456\"") == 0);
    destroy_json(&json);
}

void test_serialize_array_json()
{
    printf("\tTesting serializing JSON array...\n");
    char *vals[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR"};
    json_node_t arr;
    init_json(&arr, array);
    for (int i = 0; i < 5; i++)
    {
        json_node_t *child = (json_node_t*)malloc(sizeof(json_node_t));
        init_json(child, text);
        set_json_text_value(child, vals[i]);
        push_json_array(&arr, child);
    }
    char buffer[1024];
    serialize_json(&arr, buffer);
    assert(strcmp(buffer, "[\"ZERO\", \"ONE\", \"TWO\", \"THREE\", \"FOUR\"]") == 0);
    destroy_json(&arr);
}

void test_serialize_object_json()
{
    printf("\tTesting serializing JSON object...\n");
    char *keys[] = {"one", "two", "three", "four", "five"};
    char *vals[] = {"ichi", "nii", "san", "yon", "go"};
    json_node_t obj;
    init_json(&obj, object);
    for (int i = 0; i < 4; i++)
    {
        json_node_t *value = (json_node_t*)malloc(sizeof(json_node_t));
        init_json(value, text);
        set_json_text_value(value, vals[i]);
        set_json_object_value(&obj, keys[i], value);
    }
    char buffer[1024];
    serialize_json(&obj, buffer);
    assert(strcmp(buffer, "{\"one\": \"ichi\", \"two\": \"nii\", \"three\": \"san\", \"four\": \"yon\"}") == 0);
    destroy_json(&obj);
}

void test_serialize_large_json()
{
    printf("\tTesting serializing large JSON object...\n");
    int n = 100;
    json_node_t json;
    init_json(&json, object);
    for (int i = 0; i < n; i++)
    {
        char key[4];
        sprintf(key, "%d", i);
        json_node_t *value = (json_node_t*)malloc(sizeof(json_node_t));
        init_json(value, array);
        for (int j = 0; j < i; j++)
        {
            json_node_t *child = (json_node_t*)malloc(sizeof(json_node_t));
            init_json(child, integer);
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

    char *serialize_buffer = (char *)malloc(sizeof(char) * 20000);
    serialize_json(&json, serialize_buffer);

    assert(strcmp(large_buffer, serialize_buffer) == 0);
    destroy_json(&json);
}

void test_parse_and_serialize_json()
{
    printf("\tTesting JSON parsing...\n");
    char *s = "[\"one\", \"two\", {\"some thing\": null, \"other thing\": false, \"and the other thing\": [1, 2, 3, 11.47]}, null, [\"first\", 15, true, false]]";
    json_node_t json;
    parse_json_str(&json, s);

    char buffer[1024];
    serialize_json(&json, buffer);
    assert(strcmp(buffer, s) == 0);
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
        assert(child->type == array);
        assert(child->size == i);
        for (int j = 0; j < i; j++)
        {
            json_node_t *sub_child = get_json_array_index(child, j);
            assert(sub_child->type == integer);
            assert(get_json_integer_value(sub_child) == j);
        }
    }
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
    char buff[128];
    serialize_json(&json, buff);
    assert(res == -1);
    destroy_json(&json);
}

int main()
{
    printf("Running JSON tests...\n");
    test_json_text();
    test_json_array();
    test_json_object();

    test_serialize_text_json();
    test_serialize_array_json();
    test_serialize_object_json();
    test_serialize_large_json();

    test_parse_and_serialize_json();
    test_parse_large_json();

    test_incomplete_text_json();
    test_incomplete_array_json();
    test_multiple_comma_array_json();
    printf("All tests passed.\n");
    return 0;
}
