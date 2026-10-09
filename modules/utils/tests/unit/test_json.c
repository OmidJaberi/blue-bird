#include "blue-bird/utils/json.h"
#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void test_parse_invalid_numbers(void)
{
    printf("\tTesting invalid JSON number formats...\n");

    char *invalid[] = {
        "01",
        "-01",
        "+1",
        ".5",
        "1.",
        "1e",
        "1e+",
        "--1",
        "1abc",
        "[1, 02]",
        "[1, 2.]",
        NULL
    };

    for (int i = 0; invalid[i] != NULL; i++)
    {
        BB_ASSERT(bb_json_parse(invalid[i]) == NULL);
    }
}

void test_json_text(void)
{
    printf("\tTesting JSON text...\n");
    bb_json_t *json = bb_json_create(BB_JSON_TEXT);
    bb_json_set_value_text(json, "Hello there!");
    BB_ASSERT(bb_json_get_size(json) == 12);
    BB_ASSERT(strcmp(bb_json_get_value_text(json), "Hello there!") == 0);
    bb_json_destroy(json);
}

void test_json_array(void)
{
    printf("\tTesting JSON array...\n");
    char *vals[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR"};
    bb_json_t *arr = bb_json_create(BB_JSON_ARRAY);
    for (int i = 0; i < 5; i++)
    {
        bb_json_t *element = bb_json_create(BB_JSON_TEXT);
        bb_json_set_value_text(element, vals[i]);
        bb_json_array_push(arr, element);
    }
    BB_ASSERT(bb_json_get_size(arr) == 5);
    for (unsigned int i = 0; i < bb_json_get_size(arr); i++)
    {
        BB_ASSERT(strcmp(bb_json_get_value_text(bb_json_array_get_index(arr, i)), vals[i]) == 0);
    }
    bb_json_destroy(arr);
}

void test_bb_json_array_remove_at_index(void)
{
    printf("\tTesting JSON array remove at index...\n");
    bb_json_t *arr = bb_json_parse("[1, 2, 3, 4, 5, 6, 7, 8]");
    bb_json_array_remove_at_index(arr, 2);
    BB_ASSERT(bb_json_get_size(arr) == 7);
    char *buffer;
    int size;
    bb_json_serialize(arr, &buffer, &size);
    BB_ASSERT(strcmp(buffer, "[1, 2, 4, 5, 6, 7, 8]") == 0);
    free(buffer);
    bb_json_destroy(arr);
}

void test_json_array_multi_remove_at_index(void)
{
    printf("\tTesting JSON array remove multiple elements...\n");
    bb_json_t *arr = bb_json_create(BB_JSON_ARRAY);
    for (int i = 0; i < 1000; i++)
    {
        bb_json_array_push(arr, bb_json_new_int(i));
    }
    while (bb_json_get_size(arr) > 20)
    {
        bb_json_array_remove_at_index(arr, (unsigned int)bb_json_get_size(arr) - 1);
    }
    for (int i = 19; i > 0; i -= 2)
    {
        bb_json_array_remove_at_index(arr, i);
    }
    char *buffer;
    int size;
    bb_json_serialize(arr, &buffer, &size);
    BB_ASSERT(strcmp(buffer, "[0, 2, 4, 6, 8, 10, 12, 14, 16, 18]") == 0);
    free(buffer);
    bb_json_destroy(arr);
}

void test_json_object(void)
{
    printf("\tTesting JSON object...\n");
    char *keys[] = {"one", "two", "three", "four", "five"};
    char *vals[] = {"ichi", "nii", "san", "yon", "go"};
    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);
    for (int i = 0; i < 5; i++)
    {
        bb_json_t *value = bb_json_create(BB_JSON_TEXT);
        bb_json_set_value_text(value, vals[i]);
        bb_json_object_set_value(obj, keys[i], value);
    }
    for (unsigned int i = 0; i < bb_json_get_size(obj); i++)
    {
        bb_json_t *res = bb_json_object_get_value(obj, keys[i]);
        BB_ASSERT(strcmp(bb_json_get_value_text(res), vals[i]) == 0);
    }
    bb_json_destroy(obj);
}

void test_object_key_overwrite(void)
{
    printf("\tTesting JSON object key overwrite...\n");
    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);

    bb_json_t *v1 = bb_json_create(BB_JSON_INT);
    bb_json_set_value_integer(v1, 1);
    bb_json_object_set_value(obj, "a", v1);

    bb_json_t *v2 = bb_json_create(BB_JSON_INT);
    bb_json_set_value_integer(v2, 2);
    bb_json_object_set_value(obj, "a", v2);

    bb_json_t *res = bb_json_object_get_value(obj, "a");
    BB_ASSERT(bb_json_get_value_integer(res) == 2);

    bb_json_destroy(obj);
}

void test_parse_duplicate_object_key(void)
{
    printf("\tTesting JSON parsing of duplicate object keys...\n");

    bb_json_t *json = bb_json_parse("{\"a\": 1, \"a\": 2}");

    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_size(json) == 1);

    bb_json_t *value = bb_json_object_get_value(json, "a");

    BB_ASSERT(value != NULL);
    BB_ASSERT(bb_json_get_value_integer(value) == 2);

    bb_json_destroy(json);
}

void test_object_key_deletion(void)
{
    printf("\tTesting JSON object key deletion...\n");
    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);

    bb_json_t *v1 = bb_json_create(BB_JSON_INT);
    bb_json_set_value_integer(v1, 1);
    bb_json_object_set_value(obj, "a", v1);

    bb_json_object_remove_key(obj, "a");

    bb_json_t *res = bb_json_object_get_value(obj, "a");
    BB_ASSERT(res == NULL);

    bb_json_destroy(obj);
}

void test_serialize_integer_json(void)
{
    printf("\tTesting serializing JSON integer...\n");
    bb_json_t *json = bb_json_create(BB_JSON_INT);
    bb_json_set_value_integer(json, -123);
    char *buffer;
    int size;
    bb_json_serialize(json, &buffer, &size);
    BB_ASSERT(size == 4);
    BB_ASSERT(strcmp(buffer, "-123") == 0);
    free(buffer);
    bb_json_destroy(json);
}

void test_serialize_text_json(void)
{
    printf("\tTesting serializing JSON text...\n");
    bb_json_t *json = bb_json_create(BB_JSON_TEXT);
    bb_json_set_value_text(json, "123456");
    char *buffer;
    int size;
    bb_json_serialize(json, &buffer, &size);
    BB_ASSERT(strcmp(buffer, "\"123456\"") == 0);
    BB_ASSERT(size == 8);
    free(buffer);
    bb_json_destroy(json);
}

void test_serialize_text_json_with_escape_characters(void)
{
    printf("\tTesting serializing JSON text with escape characters...\n");
    bb_json_t *json = bb_json_create(BB_JSON_TEXT);
    bb_json_set_value_text(json, "sample text:\t12\\34\nanother line.");
    char *buffer;
    int size;
    bb_json_serialize(json, &buffer, &size);
    char *expected = "\"sample text:\\t12\\\\34\\nanother line.\"";
    BB_ASSERT(strcmp(buffer, expected) == 0);
    BB_ASSERT(size == (int)strlen(expected));
    free(buffer);
    bb_json_destroy(json);
}

void test_serialize_array_json(void)
{
    printf("\tTesting serializing JSON array...\n");
    char *vals[] = {"ZERO", "ONE", "TWO", "THREE", "FOUR"};
    bb_json_t *arr = bb_json_create(BB_JSON_ARRAY);
    for (int i = 0; i < 5; i++)
    {
        bb_json_t *child = bb_json_create(BB_JSON_TEXT);
        bb_json_set_value_text(child, vals[i]);
        bb_json_array_push(arr, child);
    }
    char *buffer;
    int size;
    bb_json_serialize(arr, &buffer, &size);
    char *expected = "[\"ZERO\", \"ONE\", \"TWO\", \"THREE\", \"FOUR\"]";
    BB_ASSERT(strcmp(buffer, expected) == 0);
    BB_ASSERT(size == (int)strlen(expected));
    free(buffer);
    bb_json_destroy(arr);
}

void test_serialize_object_json(void)
{
    printf("\tTesting serializing JSON object...\n");
    char *keys[] = {"one", "two", "three", "four", "five"};
    char *vals[] = {"ichi", "nii", "san", "yon", "go"};
    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);
    for (int i = 0; i < 4; i++)
    {
        bb_json_t *value = bb_json_create(BB_JSON_TEXT);
        bb_json_set_value_text(value, vals[i]);
        bb_json_object_set_value(obj, keys[i], value);
    }
    char *buffer;
    int size;
    bb_json_serialize(obj, &buffer, &size);
    char *expected = "{\"one\": \"ichi\", \"two\": \"nii\", \"three\": \"san\", \"four\": \"yon\"}";
    BB_ASSERT(strcmp(buffer, expected) == 0);
    BB_ASSERT(size == (int)strlen(expected));
    free(buffer);
    bb_json_destroy(obj);
}

void test_serialize_large_json(void)
{
    printf("\tTesting serializing large JSON object...\n");
    int n = 100;
    bb_json_t *json = bb_json_create(BB_JSON_OBJECT);
    for (int i = 0; i < n; i++)
    {
        char key[4];
        snprintf(key, sizeof(key), "%d", i);
        bb_json_t *value = bb_json_create(BB_JSON_ARRAY);
        for (int j = 0; j < i; j++)
        {
            bb_json_t *child = bb_json_create(BB_JSON_INT);
            bb_json_set_value_integer(child, j);
            bb_json_array_push(value, child);
        }
        bb_json_object_set_value(json, key, value);
    }
    
    char *expected = (char *)malloc(sizeof(char) * 20000);
    int index = snprintf(expected, 20000, "{");
    for (int i = 0; i < n; i++)
    {
        index += snprintf(expected + index, 20000 - index, "\"%d\": ", i);
        index += snprintf(expected + index, 20000 - index, "[");
        for (int j = 0; j < i; j++)
            index += snprintf(expected + index, 20000 - index, "%d%s", j, j < i - 1 ? ", " : "");
        index += snprintf(expected + index, 20000 - index, "]%s", i < n - 1 ? ", " : "");
    }
    index += snprintf(expected + index, 20000 - index, "}");

    char *buffer;
    int size;
    bb_json_serialize(json, &buffer, &size);

    BB_ASSERT(strcmp(buffer, expected) == 0);
    BB_ASSERT(size == (int)strlen(expected));
    free(buffer);
    free(expected);
    bb_json_destroy(json);
}

void test_parse_and_serialize(void)
{
    printf("\tTesting JSON parsing...\n");
    char *s = "[\"one\", \"two\", {\"some thing\": null, \"other thing\": false, \"and the other thing\": [1, 2, -3, 11.47]}, null, [\"first\", 15, true, false]]";
    bb_json_t *json = bb_json_parse(s);

    char *buffer;
    int size;
    bb_json_serialize(json, &buffer, &size);
    BB_ASSERT(strcmp(buffer, s) == 0);
    BB_ASSERT(size == (int)strlen(s));
    free(buffer);
    bb_json_destroy(json);
}

void test_parse_empty_text_json(void)
{
    printf("\tTesting empty text JSON parsing...\n");
    bb_json_t *json = bb_json_parse("\"\"");
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_size(json) == 0);
    bb_json_destroy(json);
}

void test_parse_empty_array_json(void)
{
    printf("\tTesting empty array JSON parsing...\n");
    bb_json_t *json = bb_json_parse("[]");
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_size(json) == 0);
    bb_json_destroy(json);
}

void test_parse_empty_object_json(void)
{
    printf("\tTesting empty object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{}");
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_size(json) == 0);
    bb_json_destroy(json);
}

void test_parse_large_json(void)
{
    printf("\tTesting large JSON parsing...\n");
    unsigned int n = 100;
    char *large_buffer = (char *)malloc(sizeof(char) * 20000);
    int index = snprintf(large_buffer, 20000, "{");
    for (unsigned int i = 0; i < n; i++)
    {
        index += snprintf(large_buffer + index, 20000 - index, "\"%d\": ", i);
        index += snprintf(large_buffer + index, 20000 - index, "[");
        for (unsigned int j = 0; j < i; j++)
            index += snprintf(large_buffer + index, 20000 - index, "%d%s", j, (j + 1) < i ? ", " : "");
        index += snprintf(large_buffer + index, 20000 - index, "]%s", (i + 1) < n ? ", " : "");
    }
    index += snprintf(large_buffer + index, 20000 - index, "}");
    bb_json_t *json = bb_json_parse(large_buffer);
    BB_ASSERT(bb_json_get_size(json) == n);
    for (unsigned int i = 0; i < n; i++)
    {
        char key[4];
        snprintf(key, sizeof(key), "%d", i);
        bb_json_t *child = bb_json_object_get_value(json, key);
        BB_ASSERT(child);
        BB_ASSERT(bb_json_get_type(child) == BB_JSON_ARRAY);
        BB_ASSERT(bb_json_get_size(child) == i);
        for (unsigned int j = 0; j < i; j++)
        {
            bb_json_t *sub_child = bb_json_array_get_index(child, j);
            BB_ASSERT(bb_json_get_type(sub_child) == BB_JSON_INT);
            BB_ASSERT(bb_json_get_value_integer(sub_child) == (int)j);
        }
    }
    free(large_buffer);
    bb_json_destroy(json);
}

void test_parse_text_with_escapes(void)
{
    printf("\tTesting parse JSON text with escapes...\n");

    // JSON source text (escaped, not raw C escapes)
    char *s = "\"hello\\nworld\\t\\u0001\"";

    bb_json_t *json = bb_json_parse(s);
    BB_ASSERT(json != NULL);

    BB_ASSERT(bb_json_get_type(json) == BB_JSON_TEXT);

    // "hello\nworld\t\x01"
    // indexes: 0123456789012
    BB_ASSERT(bb_json_get_size(json) == 13);

    char *text_val = bb_json_get_value_text(json);
    BB_ASSERT(text_val[5] == '\n');
    BB_ASSERT(text_val[11] == '\t');
    BB_ASSERT((unsigned char)text_val[12] == 0x01);

    char *buf;
    int size;
    bb_json_serialize(json, &buf, &size);
    BB_ASSERT(strcmp(buf, "\"hello\\nworld\\t\\u0001\"") == 0);
    free(buf);

    bb_json_destroy(json);
}

void test_serialize_json_size(void)
{
    printf("\tTesting serialized JSON size...\n");
    char *s = "[\"one\", \"two\", \"escape\tcharacter\", {\"some thing\": null, \"other thing\": false, \"and the other thing\": [1, 2, 3, 11.47]}, null, [\"first\", 15, true, false]]";
    bb_json_t *json = bb_json_parse(s);

    char *buffer;
    int size, null_size, str_size;
    bb_json_serialize(json, &buffer, &size);
    bb_json_serialize(json, NULL, &null_size);
    str_size = (int)strlen(buffer);
    BB_ASSERT(size == str_size);
    BB_ASSERT(null_size == str_size);
    free(buffer);
    bb_json_destroy(json);
}

// Broken JSON Parsing:
void test_incomplete_text_json(void)
{
    printf("\tTesting incomplete text JSON parsing...\n");
    bb_json_t *json = bb_json_parse("\"text with no ending quotation mark.");
    BB_ASSERT(json == NULL);
}

void test_incomplete_array_json(void)
{
    printf("\tTesting incomplete array JSON parsing...\n");
    bb_json_t *json = bb_json_parse("[1, 2, 3");
    BB_ASSERT(json == NULL);
}

void test_parse_trailing_commas(void)
{
    printf("\tTesting JSON parsing of trailing commas...\n");

    BB_ASSERT(bb_json_parse("[1, 2, 3,]") == NULL);
    BB_ASSERT(bb_json_parse("{\"one\": 1,}") == NULL);
}

void test_multiple_comma_array_json(void)
{
    printf("\tTesting multiple comma array JSON parsing...\n");
    bb_json_t *json = bb_json_parse("[1, 2, , 3]");
    BB_ASSERT(json == NULL);
}

void test_missing_comma_array_json(void)
{
    printf("\tTesting missing comma array JSON parsing...\n");
    bb_json_t *json = bb_json_parse("[1, 2 3, 4]");
    BB_ASSERT(json == NULL);
}

void test_incomplete_object_json(void)
{
    printf("\tTesting incomplete object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3");
    BB_ASSERT(json == NULL);
}

void test_multiple_comma_object_json(void)
{
    printf("\tTesting multiple comma object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, , \"two\": 2, \"three\": 3}");
    BB_ASSERT(json == NULL);
}

void test_missing_comma_object_json(void)
{
    printf("\tTesting missing comma object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1 \"two\": 2, \"three\": 3}");
    BB_ASSERT(json == NULL);
}

void test_missing_colon_object_json(void)
{
    printf("\tTesting missing colon object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, \"two\" 2, \"three\": 3}");
    BB_ASSERT(json == NULL);
}

void test_missing_value_object_json(void)
{
    printf("\tTesting missing value object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, \"two\": , \"three\": 3}");
    BB_ASSERT(json == NULL);
}

void test_missing_key_object_json(void)
{
    printf("\tTesting missing key object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, : 2 , \"three\": 3}");
    BB_ASSERT(json == NULL);
}

void test_parse_json_with_trailing_str(void)
{
    printf("\tTesting parsing object JSON with trailing str...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, \"two\": 2 , \"three\": 3}something unrelated");
    BB_ASSERT(json == NULL);
}

void test_serialize_with_non_empty_buffer(void)
{
    printf("\tTesting serializing JSON on non-empty buffer...\n");
    bb_json_t *json = bb_json_create(BB_JSON_ARRAY);
    for (int i = 0; i < 4; i++)
    {
        bb_json_array_push(json, bb_json_new_int(i));
    }
    char *buffer = malloc(sizeof(char) * 1000);
    for (int i = 0; i < 1000; i++)
    {
        buffer[i] = '#';
    }
    char *prev_buffer = buffer;
    int size;
    bb_json_serialize(json, &buffer, &size);
    BB_ASSERT(bb_strcasecmp(buffer, "[0, 1, 2, 3]") == 0);
    free(buffer);
    free(prev_buffer);
    bb_json_destroy(json);
}

void test_compare_equal_jsons(void)
{
    printf("\tTesting comparison of equal JSONs...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    BB_ASSERT(bb_json_equal(json_1, json_2));
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_compare_equal_jsons_different_order(void)
{
    printf("\tTesting comparison of equal JSONs with different order...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"three\": 3, \"two\": 2}");
    BB_ASSERT(bb_json_equal(json_1, json_2));
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_compare_jsons_missing_key(void)
{
    printf("\tTesting comparison of JSONs: missing key...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"two\": 2}");
    BB_ASSERT(!bb_json_equal(json_1, json_2));
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_compare_jsons_extra_key(void)
{
    printf("\tTesting comparison of JSONs: extra key...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    BB_ASSERT(!bb_json_equal(json_1, json_2));
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_compare_complex_equal_jsons(void)
{
    printf("\tTesting comparison of complex equal JSONs...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3, \"list\": [1, \"two\", 3.14, null, true, false, {\"name\": \"Alice\", \"age\": 30}]}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"three\": 3, \"list\": [1, \"two\", 3.14, null, true, false, {\"name\": \"Alice\", \"age\": 30}], \"two\": 2}");
    BB_ASSERT(bb_json_equal(json_1, json_2));
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_json_clone(void)
{
    printf("\tTesting JSON clone...\n");

    bb_json_t *json = bb_json_parse("{\"name\": \"Alice\", \"age\": 30, \"scores\": [10, 20, 30], \"nested\": {\"enabled\": true}}");

    bb_json_t *clone = bb_json_clone(json);

    BB_ASSERT(clone != NULL);
    BB_ASSERT(bb_json_equal(json, clone));

    bb_json_destroy(json);
    bb_json_destroy(clone);
}

void test_json_clone_independence(void)
{
    printf("\tTesting JSON clone independence...\n");

    bb_json_t *json = bb_json_parse("{\"x\": 1, \"nested\": {\"y\": 2}}");

    bb_json_t *clone = bb_json_clone(json);

    bb_json_set_value_integer(bb_json_object_get_value(clone, "x"), 100);

    bb_json_set_value_integer(bb_json_object_get_value(bb_json_object_get_value(clone, "nested"), "y"), 200);

    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(json, "x")) == 1);

    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(bb_json_object_get_value(json, "nested"), "y")) == 2);

    bb_json_destroy(json);
    bb_json_destroy(clone);
}

void test_json_object_merge(void)
{
    printf("\tTesting JSON object merge...\n");

    bb_json_t *dst = bb_json_parse("{\"a\": 1, \"b\": 2}");

    bb_json_t *src = bb_json_parse("{\"b\": 20, \"c\": 30}");

    BB_ASSERT(!BB_FAILED(bb_json_object_merge(dst, src)));

    char *buffer;
    int size;

    bb_json_serialize(dst, &buffer, &size);

    BB_ASSERT(strcmp(buffer, "{\"a\": 1, \"b\": 20, \"c\": 30}") == 0);

    free(buffer);
    bb_json_destroy(dst);
    bb_json_destroy(src);
}

void test_json_object_merge_independence(void)
{
    printf("\tTesting JSON object merge independence...\n");

    bb_json_t *dst = bb_json_parse("{\"config\": {\"port\": 8080}}");

    bb_json_t *src = bb_json_parse("{\"config\": {\"port\": 9000}}");

    BB_ASSERT(!BB_FAILED(bb_json_object_merge(dst, src)));

    bb_json_set_value_integer(bb_json_object_get_value(bb_json_object_get_value(src, "config"), "port"), 1234);

    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(bb_json_object_get_value(dst, "config"), "port")) == 9000);

    bb_json_destroy(dst);
    bb_json_destroy(src);
}

void test_json_object_merge_invalid_type(void)
{
    printf("\tTesting JSON object merge invalid type...\n");

    bb_json_t *dst = bb_json_new_array();
    bb_json_t *src = bb_json_new_object();

    BB_ASSERT(BB_FAILED(bb_json_object_merge(dst, src)));

    bb_json_destroy(dst);
    bb_json_destroy(src);
}

void test_dump_and_bb_json_load(void)
{
    printf("\tTesting JSON file load and dump...\n");
    bb_json_t *json_1;
    json_1 = bb_json_create(BB_JSON_ARRAY);
    for (int i = 0; i < 10; i++)
    {
        bb_json_t *child = bb_json_create(BB_JSON_INT);
        bb_json_set_value_integer(child, i);
        bb_json_array_push(json_1, child);
    }
    BB_ASSERT(!BB_FAILED(bb_json_dump(json_1, "test_file.json")));
    bb_json_t *json_2 = bb_json_load("test_file.json");
    char *buf;
    int size;
    bb_json_serialize(json_2, &buf, &size);

    BB_ASSERT(bb_json_get_type(json_1) == bb_json_get_type(json_2));
    BB_ASSERT(bb_json_get_size(json_1) == bb_json_get_size(json_2));
    for (unsigned int i = 0; i < bb_json_get_size(json_1); i++)
    {
        BB_ASSERT(
            bb_json_get_value_integer(bb_json_array_get_index(json_1, i))
                ==
            bb_json_get_value_integer(bb_json_array_get_index(json_2, i))
        );
    }

    free(buf);
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_json_dsl_macros(void)
{
    printf("\tTesting JSON DSL Macros...\n");
    bb_json_t *doc = BB_JSON(
        OBJ(
            KEY("name", TEXTV("Alice")),
            KEY("age", INTV(30)),
            KEY("admin", BOOLV(false)),
            KEY("scores", ARR(INTV(10), INTV(20), INTV(30))),
            KEY("misc", NULLV()),
            KEY("nested",
                OBJ(
                    KEY("pi", REALV((float)3.14)),
                    KEY("ok", BOOLV(true))
                )
            )
        )
    );
    char *buf;
    int size;
    bb_json_serialize(doc, &buf, &size);
    BB_ASSERT(strcmp(buf, "{\"name\": \"Alice\", \"age\": 30, \"admin\": false, \"scores\": [10, 20, 30], \"misc\": null, \"nested\": {\"pi\": 3.14, \"ok\": true}}") == 0);
    free(buf);
    bb_json_destroy(doc);
}

void test_json_type_mismatch(void)
{
    printf("\tTesting JSON type mismatch handling...\n");

    bb_json_t *num = bb_json_new_int(7);
    bb_json_t *text = bb_json_new_text("abc");
    bb_json_t *arr = bb_json_new_array();
    bb_json_t *obj = bb_json_new_object();

    // Setters reject values of the wrong type and leave the node untouched.
    bb_error_t err = bb_json_set_value_text(num, "nope");
    BB_ASSERT(err.code == BB_ERR_JSON_TYPE_MISMATCH);
    err = bb_json_set_value_bool(text, true);
    BB_ASSERT(err.code == BB_ERR_JSON_TYPE_MISMATCH);
    err = bb_json_set_value_real(num, 1.5f);
    BB_ASSERT(err.code == BB_ERR_JSON_TYPE_MISMATCH);
    BB_ASSERT(bb_json_get_value_integer(num) == 7);
    BB_ASSERT(strcmp(bb_json_get_value_text(text), "abc") == 0);

    // Getters on the wrong type return neutral defaults.
    BB_ASSERT(bb_json_get_value_text(num) == NULL);
    BB_ASSERT(bb_json_get_value_integer(text) == 0);
    BB_ASSERT(bb_json_get_value_bool(num) == false);

    // Container operations reject the wrong container type.
    // (a rejected element is not adopted, so the caller still owns it)
    bb_json_t *orphan = bb_json_new_int(1);
    err = bb_json_array_push(obj, orphan);
    BB_ASSERT(err.code == BB_ERR_JSON_TYPE_MISMATCH);
    err = bb_json_object_set_value(arr, "k", orphan);
    BB_ASSERT(err.code == BB_ERR_JSON_TYPE_MISMATCH);
    bb_json_destroy(orphan);
    BB_ASSERT(bb_json_array_get_index(obj, 0) == NULL);
    BB_ASSERT(bb_json_object_get_value(arr, "k") == NULL);

    // Out-of-range array access.
    bb_json_array_push(arr, bb_json_new_int(1));
    BB_ASSERT(bb_json_array_get_index(arr, 1) == NULL);
    err = bb_json_array_remove_at_index(arr, 1);
    BB_ASSERT(err.code == BB_ERR_JSON_OVERFLOW);
    BB_ASSERT(bb_json_get_size(arr) == 1);

    bb_json_destroy(num);
    bb_json_destroy(text);
    bb_json_destroy(arr);
    bb_json_destroy(obj);
}

void test_object_remove_missing_key(void)
{
    printf("\tTesting JSON object remove of missing key...\n");
    bb_json_t *obj = bb_json_parse("{\"a\": 1, \"b\": 2, \"c\": 3}");
    BB_ASSERT(obj != NULL);

    bb_error_t err = bb_json_object_remove_key(obj, "zzz");
    BB_ASSERT(err.code == BB_ERR_NOT_FOUND);
    BB_ASSERT(bb_json_get_size(obj) == 3);

    // Removing a middle key keeps the insertion order of the rest.
    BB_ASSERT(!BB_FAILED(bb_json_object_remove_key(obj, "b")));
    BB_ASSERT(bb_json_get_size(obj) == 2);
    BB_ASSERT(bb_json_object_get_value(obj, "b") == NULL);

    char *buffer;
    int size;
    bb_json_serialize(obj, &buffer, &size);
    BB_ASSERT(strcmp(buffer, "{\"a\": 1, \"c\": 3}") == 0);
    free(buffer);

    // Removing the same key twice fails the second time.
    err = bb_json_object_remove_key(obj, "b");
    BB_ASSERT(err.code == BB_ERR_NOT_FOUND);

    bb_json_destroy(obj);
}

void test_parse_literals_and_whitespace(void)
{
    printf("\tTesting JSON parsing of literals and surrounding whitespace...\n");
    bb_json_t *json = bb_json_parse("  \n\t[true, false, null, -7, 2.5]  \n");
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_type(json) == BB_JSON_ARRAY);
    BB_ASSERT(bb_json_get_size(json) == 5);

    BB_ASSERT(bb_json_get_type(bb_json_array_get_index(json, 0)) == BB_JSON_BOOL);
    BB_ASSERT(bb_json_get_value_bool(bb_json_array_get_index(json, 0)) == true);
    BB_ASSERT(bb_json_get_value_bool(bb_json_array_get_index(json, 1)) == false);
    BB_ASSERT(bb_json_get_type(bb_json_array_get_index(json, 2)) == BB_JSON_NULL);
    BB_ASSERT(bb_json_get_type(bb_json_array_get_index(json, 3)) == BB_JSON_INT);
    BB_ASSERT(bb_json_get_value_integer(bb_json_array_get_index(json, 3)) == -7);
    BB_ASSERT(bb_json_get_type(bb_json_array_get_index(json, 4)) == BB_JSON_REAL);
    BB_ASSERT(bb_json_get_value_real(bb_json_array_get_index(json, 4)) == 2.5f);
    bb_json_destroy(json);

    // Bare top-level literals.
    json = bb_json_parse("null");
    BB_ASSERT(json != NULL && bb_json_get_type(json) == BB_JSON_NULL);
    bb_json_destroy(json);

    // Misspelled / truncated literals and empty input are rejected.
    BB_ASSERT(bb_json_parse("nul") == NULL);
    BB_ASSERT(bb_json_parse("tru") == NULL);
    BB_ASSERT(bb_json_parse("[fals]") == NULL);
    BB_ASSERT(bb_json_parse("") == NULL);
    BB_ASSERT(bb_json_parse("   ") == NULL);
}

void test_serialize_indented_roundtrip(void)
{
    printf("\tTesting indented JSON serialization roundtrip...\n");
    bb_json_t *doc = bb_json_parse(
        "{\"name\": \"Alice\", \"tags\": [\"a\", \"b\"], "
        "\"nested\": {\"list\": [{\"x\": 1}, {\"x\": 2}], \"empty\": [], \"none\": {}}, "
        "\"ok\": true, \"gone\": null}");
    BB_ASSERT(doc != NULL);

    // serialize_indented frees a non-NULL *buffer, so it must start as NULL.
    char *buffer = NULL;
    int size = 0;
    BB_ASSERT(!BB_FAILED(bb_json_serialize_indented(doc, &buffer, &size)));
    BB_ASSERT(buffer != NULL);
    BB_ASSERT(size == (int)strlen(buffer));
    BB_ASSERT(strchr(buffer, '\n') != NULL);

    bb_json_t *reparsed = bb_json_parse(buffer);
    BB_ASSERT(reparsed != NULL);
    BB_ASSERT(bb_json_equal(doc, reparsed));

    free(buffer);
    bb_json_destroy(doc);
    bb_json_destroy(reparsed);
}

void test_json_load_missing_file(void)
{
    printf("\tTesting JSON load of missing file...\n");
    BB_ASSERT(bb_json_load("this_file_should_not_exist_bb.json") == NULL);
}

int main(void)
{
    printf("Running JSON tests...\n");
    test_parse_invalid_numbers();
    test_json_text();
    test_json_array();
    test_bb_json_array_remove_at_index();
    test_json_array_multi_remove_at_index();
    test_json_object();
    test_object_key_overwrite();
    test_parse_duplicate_object_key();
    test_object_key_deletion();
    test_object_remove_missing_key();
    test_json_type_mismatch();

    test_serialize_integer_json();
    test_serialize_text_json();
    test_serialize_array_json();
    test_serialize_object_json();
    test_serialize_large_json();

    test_parse_and_serialize();
    test_parse_empty_text_json();
    test_parse_empty_array_json();
    test_parse_empty_object_json();
    test_parse_large_json();
    test_parse_text_with_escapes();
    test_serialize_json_size();
    test_parse_literals_and_whitespace();

    test_incomplete_text_json();
    test_incomplete_array_json();
    test_parse_trailing_commas();
    test_multiple_comma_array_json();
    test_missing_comma_array_json();
    test_incomplete_object_json();
    test_multiple_comma_object_json();
    test_missing_comma_object_json();
    test_missing_colon_object_json();
    test_missing_value_object_json();
    test_missing_key_object_json();
    test_parse_json_with_trailing_str();
    test_serialize_with_non_empty_buffer();

    test_compare_equal_jsons();
    test_compare_equal_jsons_different_order();
    test_compare_jsons_missing_key();
    test_compare_jsons_extra_key();
    test_compare_complex_equal_jsons();

    test_json_clone();
    test_json_clone_independence();
    test_json_object_merge();
    test_json_object_merge_independence();
    test_json_object_merge_invalid_type();

    test_dump_and_bb_json_load();
    test_json_load_missing_file();
    test_serialize_indented_roundtrip();

    test_json_dsl_macros();
    printf("All tests passed.\n");
    return 0;
}
