#include "utils/json.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

void test_json_text()
{
    printf("\ttesting JSON text...\n");
    json_node_t json;
    init_json(&json, text);
    set_json_text_value(&json, "Hello there!");
    assert(json.size == 12);
    assert(strcmp(get_json_text_value(&json), "Hello there!") == 0);
}

void test_json_array()
{
    printf("\ttesting JSON array...\n");
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
}

void test_json_object()
{
    printf("\ttesting JSON object...\n");
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
}

int main()
{
    printf("Running JSON tests...\n");
    test_json_text();
    test_json_array();
    test_json_object();
    printf("All tests passed.\n");
    return 0;
}
