#include "blue-bird/utils/json.h"
#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <math.h>

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

void test_json_array_remove_boundaries(void)
{
    printf("\tTesting JSON array removal at boundaries...\n");

    bb_json_t *arr = bb_json_parse("[10, 20, 30, 40, 50]");
    BB_ASSERT(arr != NULL);

    BB_ASSERT(!BB_FAILED(bb_json_array_remove_at_index(arr, 0)));
    BB_ASSERT(bb_json_get_size(arr) == 4);
    BB_ASSERT(bb_json_get_value_integer(bb_json_array_get_index(arr, 0)) == 20);

    BB_ASSERT(!BB_FAILED(
        bb_json_array_remove_at_index(arr, bb_json_get_size(arr) - 1)
    ));

    BB_ASSERT(bb_json_get_size(arr) == 3);
    BB_ASSERT(bb_json_get_value_integer(bb_json_array_get_index(arr, 0)) == 20);
    BB_ASSERT(bb_json_get_value_integer(bb_json_array_get_index(arr, 1)) == 30);
    BB_ASSERT(bb_json_get_value_integer(bb_json_array_get_index(arr, 2)) == 40);

    bb_json_destroy(arr);
}

void test_json_array_remove_empty_and_reuse(void)
{
    printf("\tTesting JSON array removal on empty array and reuse after emptying...\n");

    bb_json_t *arr = bb_json_new_array();

    // Removing from an empty array must fail cleanly (no unsigned underflow, no crash).
    bb_error_t err = bb_json_array_remove_at_index(arr, 0);
    BB_ASSERT(err.code == BB_ERR_JSON_OVERFLOW);
    BB_ASSERT(bb_json_get_size(arr) == 0);
    BB_ASSERT(bb_json_array_get_index(arr, 0) == NULL);
    BB_ASSERT(bb_json_array_get_index(arr, UINT_MAX) == NULL);

    // Remove the only element, then the array must still be usable.
    BB_ASSERT(!BB_FAILED(bb_json_array_push(arr, bb_json_new_int(42))));
    BB_ASSERT(bb_json_get_size(arr) == 1);
    BB_ASSERT(!BB_FAILED(bb_json_array_remove_at_index(arr, 0)));
    BB_ASSERT(bb_json_get_size(arr) == 0);

    err = bb_json_array_remove_at_index(arr, 0);
    BB_ASSERT(err.code == BB_ERR_JSON_OVERFLOW);

    for (int i = 0; i < 50; i++)
        BB_ASSERT(!BB_FAILED(bb_json_array_push(arr, bb_json_new_int(i))));
    BB_ASSERT(bb_json_get_size(arr) == 50);
    BB_ASSERT(bb_json_get_value_integer(bb_json_array_get_index(arr, 0)) == 0);
    BB_ASSERT(bb_json_get_value_integer(bb_json_array_get_index(arr, 49)) == 49);

    // Empty it completely again and make sure it serializes as an empty array.
    while (bb_json_get_size(arr) > 0)
        BB_ASSERT(!BB_FAILED(bb_json_array_remove_at_index(arr, 0)));

    char *buffer = NULL;
    int size = 0;
    BB_ASSERT(!BB_FAILED(bb_json_serialize(arr, &buffer, &size)));
    BB_ASSERT(strcmp(buffer, "[]") == 0);
    BB_ASSERT(size == 2);
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

void test_parse_integer_boundaries(void)
{
    printf("\tTesting JSON integer boundaries...\n");

    char buffer[64];

    snprintf(buffer, sizeof(buffer), "%d", INT_MAX);
    bb_json_t *json = bb_json_parse(buffer);
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_type(json) == BB_JSON_INT);
    BB_ASSERT(bb_json_get_value_integer(json) == INT_MAX);
    bb_json_destroy(json);

    snprintf(buffer, sizeof(buffer), "%d", INT_MIN);
    json = bb_json_parse(buffer);
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_type(json) == BB_JSON_INT);
    BB_ASSERT(bb_json_get_value_integer(json) == INT_MIN);
    bb_json_destroy(json);

    BB_ASSERT(bb_json_parse("2147483648") == NULL);
    BB_ASSERT(bb_json_parse("-2147483649") == NULL);
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
    char *s = "[\"one\", \"two\", \"escape\\tcharacter\", {\"some thing\": null, \"other thing\": false, \"and the other thing\": [1, 2, 3, 11.47]}, null, [\"first\", 15, true, false]]";
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

void test_compare_jsons_different_structure(void)
{
    printf("\tTesting comparison of structurally different JSONs...\n");

    // Same size, same values, but different keys (catches one-directional key lookups).
    bb_json_t *a = bb_json_parse("{\"a\": 1, \"b\": 2}");
    bb_json_t *b = bb_json_parse("{\"a\": 1, \"c\": 2}");
    BB_ASSERT(a != NULL && b != NULL);
    BB_ASSERT(!bb_json_equal(a, b));
    BB_ASSERT(!bb_json_equal(b, a));
    bb_json_destroy(a);
    bb_json_destroy(b);

    // Same elements, different order: arrays are ordered.
    a = bb_json_parse("[1, 2, 3]");
    b = bb_json_parse("[3, 2, 1]");
    BB_ASSERT(a != NULL && b != NULL);
    BB_ASSERT(!bb_json_equal(a, b));
    bb_json_destroy(a);
    bb_json_destroy(b);

    // Different lengths with a common prefix.
    a = bb_json_parse("[1, 2]");
    b = bb_json_parse("[1, 2, 3]");
    BB_ASSERT(!bb_json_equal(a, b));
    BB_ASSERT(!bb_json_equal(b, a));
    bb_json_destroy(a);
    bb_json_destroy(b);

    // Different types that look alike.
    a = bb_json_parse("null");
    b = bb_json_parse("false");
    BB_ASSERT(!bb_json_equal(a, b));
    bb_json_destroy(a);
    bb_json_destroy(b);

    a = bb_json_parse("\"1\"");
    b = bb_json_parse("1");
    BB_ASSERT(!bb_json_equal(a, b));
    bb_json_destroy(a);
    bb_json_destroy(b);

    a = bb_json_parse("[]");
    b = bb_json_parse("{}");
    BB_ASSERT(!bb_json_equal(a, b));
    bb_json_destroy(a);
    bb_json_destroy(b);

    // A single difference buried deep inside an otherwise identical document.
    a = bb_json_parse("{\"x\": [1, {\"y\": [true, {\"z\": \"deep\"}]}]}");
    b = bb_json_parse("{\"x\": [1, {\"y\": [true, {\"z\": \"deeP\"}]}]}");
    BB_ASSERT(a != NULL && b != NULL);
    BB_ASSERT(!bb_json_equal(a, b));
    bb_json_destroy(a);
    bb_json_destroy(b);
}

void test_json_deep_nesting(void)
{
    printf("\tTesting deeply nested JSON...\n");

    const int depth = 256;
    char *buffer = malloc((size_t)(depth * 2 + 2));
    BB_ASSERT(buffer != NULL);

    for (int i = 0; i < depth; i++)
        buffer[i] = '[';

    buffer[depth] = '0';

    for (int i = 0; i < depth; i++)
        buffer[depth + 1 + i] = ']';

    buffer[depth * 2 + 1] = '\0';

    bb_json_t *json = bb_json_parse(buffer);
    BB_ASSERT(json != NULL);

    char *serialized = NULL;
    int size = 0;
    bb_json_serialize(json, &serialized, &size);

    BB_ASSERT(serialized != NULL);
    BB_ASSERT(strcmp(buffer, serialized) == 0);
    BB_ASSERT(size == (int)strlen(buffer));

    bb_json_t *clone = bb_json_clone(json);
    BB_ASSERT(clone != NULL);
    BB_ASSERT(bb_json_equal(json, clone));

    bb_json_destroy(clone);
    bb_json_destroy(json);
    free(serialized);
    free(buffer);
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

void test_json_clone_array_independence(void)
{
    printf("\tTesting JSON clone array independence...\n");

    bb_json_t *json = bb_json_parse(
        "{\"values\": [10, 20, 30], \"nested\": [[1, 2], [3, 4]]}"
    );
    BB_ASSERT(json != NULL);

    bb_json_t *clone = bb_json_clone(json);
    BB_ASSERT(clone != NULL);

    bb_json_t *clone_values = bb_json_object_get_value(clone, "values");
    bb_json_t *clone_nested = bb_json_object_get_value(clone, "nested");

    bb_json_set_value_integer(bb_json_array_get_index(clone_values, 0), 100);
    BB_ASSERT(!BB_FAILED(bb_json_array_remove_at_index(clone_values, 1)));
    bb_json_set_value_integer(
        bb_json_array_get_index(bb_json_array_get_index(clone_nested, 0), 0),
        999
    );

    bb_json_t *original_values = bb_json_object_get_value(json, "values");
    bb_json_t *original_nested = bb_json_object_get_value(json, "nested");

    BB_ASSERT(bb_json_get_size(original_values) == 3);
    BB_ASSERT(bb_json_get_value_integer(
        bb_json_array_get_index(original_values, 0)
    ) == 10);
    BB_ASSERT(bb_json_get_value_integer(
        bb_json_array_get_index(original_values, 1)
    ) == 20);

    BB_ASSERT(bb_json_get_value_integer(
        bb_json_array_get_index(
            bb_json_array_get_index(original_nested, 0), 0
        )
    ) == 1);

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

void test_json_control_characters_roundtrip(void)
{
    printf("\tTesting JSON control character roundtrip...\n");

    char value[32];
    for (int i = 1; i < 32; i++)
        value[i - 1] = (char)i;
    value[31] = '\0';

    bb_json_t *json = bb_json_new_text(value);

    char *buffer = NULL;
    int size = 0;
    bb_json_serialize(json, &buffer, &size);

    BB_ASSERT(buffer != NULL);
    BB_ASSERT(size == (int)strlen(buffer));

    bb_json_t *parsed = bb_json_parse(buffer);
    BB_ASSERT(parsed != NULL);
    BB_ASSERT(bb_json_equal(json, parsed));

    free(buffer);
    bb_json_destroy(parsed);
    bb_json_destroy(json);
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

// ---------------------------------------------------------------------------
// Standard JSON (RFC 8259) conformance tests
// ---------------------------------------------------------------------------

// Parses a copy of `text` that lives in an exactly-sized heap block, so sanitizers
// catch any read past the terminating NUL.
static bb_json_t *parse_exact(const char *text)
{
    size_t n = strlen(text);
    char *copy = malloc(n + 1);
    BB_ASSERT(copy != NULL);
    memcpy(copy, text, n + 1);
    bb_json_t *json = bb_json_parse(copy);
    free(copy);
    return json;
}

static void assert_rejects(const char *text)
{
    bb_json_t *json = parse_exact(text);
    if (json != NULL)
    {
        fprintf(stderr, "Expected parse failure for: %s\n", text);
        bb_json_destroy(json);
        BB_ASSERT(false);
    }
}

static bb_json_t *assert_accepts(const char *text)
{
    bb_json_t *json = parse_exact(text);
    if (json == NULL)
        fprintf(stderr, "Expected parse success for: %s\n", text);
    BB_ASSERT(json != NULL);
    return json;
}

// Asserts that `json` is a text node holding exactly the `len` bytes of `expected`.
static void assert_text_bytes(bb_json_t *json, const char *expected, size_t len)
{
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_type(json) == BB_JSON_TEXT);
    BB_ASSERT(bb_json_get_size(json) == len);
    BB_ASSERT(memcmp(bb_json_get_value_text(json), expected, len + 1) == 0);
}

static void assert_parses_to_text(const char *json_text, const char *expected, size_t len)
{
    bb_json_t *json = assert_accepts(json_text);
    assert_text_bytes(json, expected, len);
    bb_json_destroy(json);
}

static void assert_serializes_to(bb_json_t *json, const char *expected)
{
    char *buffer = NULL;
    int size = 0, size_only = 0;
    BB_ASSERT(!BB_FAILED(bb_json_serialize(json, &buffer, &size)));
    bb_json_serialize(json, NULL, &size_only);
    if (strcmp(buffer, expected) != 0)
        fprintf(stderr, "Serialized: %s\nExpected:   %s\n", buffer, expected);
    BB_ASSERT(strcmp(buffer, expected) == 0);
    BB_ASSERT(size == (int)strlen(expected));
    BB_ASSERT(size_only == size);
    free(buffer);
}

void test_parse_valid_standard_documents(void)
{
    printf("\tTesting parsing of RFC 8259 example documents...\n");

    bb_json_t *doc = assert_accepts(
        "{\n"
        "  \"Image\": {\n"
        "    \"Width\":  800,\n"
        "    \"Height\": 600,\n"
        "    \"Title\":  \"View from 15th Floor\",\n"
        "    \"Thumbnail\": {\n"
        "      \"Url\":    \"http://www.example.com/image/481989943\",\n"
        "      \"Height\": 125,\n"
        "      \"Width\":  100\n"
        "    },\n"
        "    \"Animated\" : false,\n"
        "    \"IDs\": [116, 943, 234, 38793]\n"
        "  }\n"
        "}");
    bb_json_t *image = bb_json_object_get_value(doc, "Image");
    BB_ASSERT(image != NULL);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(image, "Width")) == 800);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(image, "Height")) == 600);
    BB_ASSERT(strcmp(bb_json_get_value_text(bb_json_object_get_value(image, "Title")), "View from 15th Floor") == 0);
    BB_ASSERT(bb_json_get_value_bool(bb_json_object_get_value(image, "Animated")) == false);
    bb_json_t *thumb = bb_json_object_get_value(image, "Thumbnail");
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(thumb, "Width")) == 100);
    bb_json_t *ids = bb_json_object_get_value(image, "IDs");
    BB_ASSERT(bb_json_get_size(ids) == 4);
    BB_ASSERT(bb_json_get_value_integer(bb_json_array_get_index(ids, 3)) == 38793);
    bb_json_destroy(doc);

    bb_json_t *list = assert_accepts(
        "[{\"precision\": \"zip\", \"Latitude\": 37.7668, \"Longitude\": -122.3959,"
        "  \"Address\": \"\", \"City\": \"SAN FRANCISCO\", \"State\": \"CA\", \"Zip\": \"94107\", \"Country\": \"US\"},"
        " {\"precision\": \"zip\", \"Latitude\": 37.371991, \"Longitude\": -122.026020,"
        "  \"Address\": \"\", \"City\": \"SUNNYVALE\", \"State\": \"CA\", \"Zip\": \"94085\", \"Country\": \"US\"}]");
    BB_ASSERT(bb_json_get_size(list) == 2);
    bb_json_t *first = bb_json_array_get_index(list, 0);
    BB_ASSERT(bb_json_get_type(bb_json_object_get_value(first, "Latitude")) == BB_JSON_REAL);
    BB_ASSERT(bb_json_get_value_real(bb_json_object_get_value(first, "Latitude")) == 37.7668f);
    BB_ASSERT(bb_json_get_value_real(bb_json_object_get_value(first, "Longitude")) == -122.3959f);
    BB_ASSERT(bb_json_get_size(bb_json_object_get_value(first, "Address")) == 0);
    bb_json_destroy(list);

    // Bare top-level scalars are valid JSON texts (RFC 8259 section 2).
    bb_json_t *scalar = assert_accepts("true");
    BB_ASSERT(bb_json_get_value_bool(scalar) == true);
    bb_json_destroy(scalar);
    scalar = assert_accepts("false");
    BB_ASSERT(bb_json_get_type(scalar) == BB_JSON_BOOL && bb_json_get_value_bool(scalar) == false);
    bb_json_destroy(scalar);
    scalar = assert_accepts(" \"just a string\" ");
    assert_text_bytes(scalar, "just a string", 13);
    bb_json_destroy(scalar);
    scalar = assert_accepts("0");
    BB_ASSERT(bb_json_get_type(scalar) == BB_JSON_INT && bb_json_get_value_integer(scalar) == 0);
    bb_json_destroy(scalar);
}

void test_parse_exponent_numbers(void)
{
    printf("\tTesting JSON numbers with exponents...\n");

    struct { const char *text; float expected; } cases[] = {
        { "1e2", 100.0f },     { "1E2", 100.0f },      { "1e+2", 100.0f },
        { "1e-2", 0.01f },     { "-1.5e3", -1500.0f }, { "2.5E+2", 250.0f },
        { "0e0", 0.0f },       { "0.5e1", 5.0f },      { "1e0", 1.0f },
        { "123e-2", 1.23f },   { "-0e5", -0.0f },      { "1.0e+00", 1.0f },
        { "1e038", 1e38f },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        bb_json_t *json = assert_accepts(cases[i].text);
        // A number with an exponent is always a real, even if its value is integral.
        BB_ASSERT(bb_json_get_type(json) == BB_JSON_REAL);
        BB_ASSERT(bb_json_get_value_real(json) == cases[i].expected);
        bb_json_destroy(json);
    }

    bb_json_t *arr = assert_accepts("[1, 1.0, 1e0, -2E-1,3]");
    BB_ASSERT(bb_json_get_size(arr) == 5);
    BB_ASSERT(bb_json_get_type(bb_json_array_get_index(arr, 0)) == BB_JSON_INT);
    BB_ASSERT(bb_json_get_type(bb_json_array_get_index(arr, 1)) == BB_JSON_REAL);
    BB_ASSERT(bb_json_get_type(bb_json_array_get_index(arr, 2)) == BB_JSON_REAL);
    BB_ASSERT(bb_json_get_value_real(bb_json_array_get_index(arr, 3)) == -0.2f);
    BB_ASSERT(bb_json_get_type(bb_json_array_get_index(arr, 4)) == BB_JSON_INT);
    bb_json_destroy(arr);

    bb_json_t *obj = assert_accepts("{\"a\":1E1,\"b\":2e-1}");
    BB_ASSERT(bb_json_get_value_real(bb_json_object_get_value(obj, "a")) == 10.0f);
    BB_ASSERT(bb_json_get_value_real(bb_json_object_get_value(obj, "b")) == 0.2f);
    bb_json_destroy(obj);

    char *invalid[] = {
        "1e", "1E", "1e+", "1e-", "1.e2", ".e2", "e2", "E5", "1e+-2", "1e2.5",
        "1e2e3", "0e", "-e1", "1ee2", "1e 2", "1 e2", "[1e]", "[1e2,]", "[1e+]",
        "1.5e", "1.5e+", "--1e2", "+1e2", "1e++2", "0e-", NULL
    };
    for (int i = 0; invalid[i] != NULL; i++)
        assert_rejects(invalid[i]);
}

void test_parse_number_range_and_forms(void)
{
    printf("\tTesting JSON number range and unusual forms...\n");

    // Overflow cannot be represented as a float; underflow just rounds toward zero.
    assert_rejects("1e999");
    assert_rejects("-1e999");
    assert_rejects("4e38");
    bb_json_t *json = assert_accepts("1e-999");
    BB_ASSERT(bb_json_get_type(json) == BB_JSON_REAL);
    BB_ASSERT(bb_json_get_value_real(json) == 0.0f);
    bb_json_destroy(json);
    json = assert_accepts("1e38");
    BB_ASSERT(bb_json_get_value_real(json) == 1e38f);
    bb_json_destroy(json);

    // -0 is an integer zero; -0.0 is a real.
    json = assert_accepts("-0");
    BB_ASSERT(bb_json_get_type(json) == BB_JSON_INT && bb_json_get_value_integer(json) == 0);
    bb_json_destroy(json);
    json = assert_accepts("-0.0");
    BB_ASSERT(bb_json_get_type(json) == BB_JSON_REAL && bb_json_get_value_real(json) == 0.0f);
    bb_json_destroy(json);
    json = assert_accepts("[0,-1,0.0,0.5,-0.5]");
    BB_ASSERT(bb_json_get_size(json) == 5);
    bb_json_destroy(json);

    // Numbers with very many digits must not be truncated or rejected for length.
    char long_real[512];
    snprintf(long_real, sizeof(long_real), "1.");
    for (int i = 0; i < 300; i++)
        strcat(long_real, "0");
    json = assert_accepts(long_real);
    BB_ASSERT(bb_json_get_type(json) == BB_JSON_REAL);
    BB_ASSERT(bb_json_get_value_real(json) == 1.0f);
    bb_json_destroy(json);

    snprintf(long_real, sizeof(long_real), "0.");
    for (int i = 0; i < 300; i++)
        strcat(long_real, "0");
    strcat(long_real, "1");
    json = assert_accepts(long_real);
    BB_ASSERT(bb_json_get_type(json) == BB_JSON_REAL);
    bb_json_destroy(json);

    // ...while an integer that long is simply out of range.
    char long_int[256] = "1";
    for (int i = 0; i < 150; i++)
        strcat(long_int, "0");
    assert_rejects(long_int);

    // Forms that strtof/strtol would accept but JSON does not.
    char *invalid[] = {
        "0x10", "0X1F", "0b1", "1_000", "Infinity", "-Infinity", "NaN", "nan", "inf",
        "-inf", "-", "-a", "[-]", "- 1", "1.5.5", "1..5", "0.", "-.5", "00", "[00]",
        "1,5", "١٢٣", NULL
    };
    for (int i = 0; invalid[i] != NULL; i++)
        assert_rejects(invalid[i]);
}

void test_parse_unicode_escapes(void)
{
    printf("\tTesting JSON \\u escapes are decoded to UTF-8...\n");

    assert_parses_to_text("\"\\u0041\"", "A", 1);
    assert_parses_to_text("\"\\u00e9\"", "\xc3\xa9", 2);
    assert_parses_to_text("\"\\u00E9\"", "\xc3\xa9", 2);
    assert_parses_to_text("\"\\u20ac\"", "\xe2\x82\xac", 3);
    assert_parses_to_text("\"\\u20AC\"", "\xe2\x82\xac", 3);
    assert_parses_to_text("\"a\\u00e9\\u20acz\"", "a\xc3\xa9\xe2\x82\xac" "z", 7);

    // Encoding boundaries
    assert_parses_to_text("\"\\u007f\"", "\x7f", 1);
    assert_parses_to_text("\"\\u0080\"", "\xc2\x80", 2);
    assert_parses_to_text("\"\\u07ff\"", "\xdf\xbf", 2);
    assert_parses_to_text("\"\\u0800\"", "\xe0\xa0\x80", 3);
    assert_parses_to_text("\"\\ud7ff\"", "\xed\x9f\xbf", 3);
    assert_parses_to_text("\"\\ue000\"", "\xee\x80\x80", 3);
    assert_parses_to_text("\"\\uffff\"", "\xef\xbf\xbf", 3);
    assert_parses_to_text("\"\\u0001\\u001f\"", "\x01\x1f", 2);

    // Surrogate pairs combine into a single 4-byte code point.
    assert_parses_to_text("\"\\ud83d\\ude00\"", "\xf0\x9f\x98\x80", 4);
    assert_parses_to_text("\"\\uD83D\\uDE00\"", "\xf0\x9f\x98\x80", 4);
    assert_parses_to_text("\"\\ud800\\udc00\"", "\xf0\x90\x80\x80", 4);
    assert_parses_to_text("\"\\udbff\\udfff\"", "\xf4\x8f\xbf\xbf", 4);
    assert_parses_to_text("\"x\\ud83d\\ude00y\"", "x\xf0\x9f\x98\x80y", 6);

    // Raw UTF-8 passes through untouched.
    assert_parses_to_text("\"\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80\"", "\xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80", 9);

    char *invalid[] = {
        "\"\\u0000\"",             // U+0000 cannot live in a C string
        "\"\\ud83d\"",             // lone high surrogate
        "\"\\ud83dx\"",            // high surrogate followed by plain text
        "\"\\ud83d\\u0041\"",      // high surrogate followed by non-surrogate
        "\"\\ud83d\\ud83d\"",      // two high surrogates
        "\"\\ud83d\\\\ude00\"",    // escaped backslash instead of \u
        "\"\\ude00\"",             // lone low surrogate
        "\"\\ude00\\ud83d\"",      // reversed pair
        "\"\\u12\"", "\"\\u123\"", "\"\\u12G4\"", "\"\\u 123\"", "\"\\u\"", "\"\\u",
        "\"\\U0041\"",
        NULL
    };
    for (int i = 0; invalid[i] != NULL; i++)
        assert_rejects(invalid[i]);
}

void test_parse_string_escape_validation(void)
{
    printf("\tTesting JSON string escape validation...\n");

    assert_parses_to_text("\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"", "\"\\/\b\f\n\r\t", 8);
    assert_parses_to_text("\"\\\\\"", "\\", 1);
    assert_parses_to_text("\"\\\\n\"", "\\n", 2);
    assert_parses_to_text("\"a\\\"b\"", "a\"b", 3);

    char *invalid[] = {
        "\"\\x41\"", "\"\\a\"", "\"\\'\"", "\"\\0\"", "\"\\ \"", "\"\\v\"", "\"\\e\"",
        "\"abc\\", "\"abc\\\"", "\"abc", "\"", "[\"abc]", "{\"a\":\"b}", NULL
    };
    for (int i = 0; invalid[i] != NULL; i++)
        assert_rejects(invalid[i]);

    // Single-quoted strings are not JSON.
    assert_rejects("'abc'");
    assert_rejects("['abc']");
    assert_rejects("{'a': 1}");
}

void test_parse_rejects_raw_control_characters(void)
{
    printf("\tTesting raw control characters inside JSON strings...\n");

    // U+0000..U+001F must be escaped inside strings, as values and as object keys.
    for (int c = 1; c < 0x20; c++)
    {
        char text[16];
        snprintf(text, sizeof(text), "\"a%cb\"", c);
        assert_rejects(text);
        snprintf(text, sizeof(text), "{\"a%cb\":1}", c);
        assert_rejects(text);
        snprintf(text, sizeof(text), "[\"%c\"]", c);
        assert_rejects(text);
    }

    // DEL and non-ASCII bytes are legal unescaped.
    assert_parses_to_text("\"a\x7f" "b\"", "a\x7f" "b", 3);
}

void test_parse_whitespace_rules(void)
{
    printf("\tTesting JSON insignificant whitespace rules...\n");

    bb_json_t *json = assert_accepts("\r\n\t [ \r\n 1 \t,\n 2 ] \r\n");
    BB_ASSERT(bb_json_get_size(json) == 2);
    bb_json_destroy(json);

    json = assert_accepts("{ \"a\" \n:\r\t 1 , \"b\" : [ ] ,\"c\":{ } }");
    BB_ASSERT(bb_json_get_size(json) == 3);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(json, "a")) == 1);
    bb_json_destroy(json);

    // Only space, \t, \n and \r are whitespace.
    assert_rejects("\f[1]");
    assert_rejects("\v[1]");
    assert_rejects("[1]\f");
    assert_rejects("[\v1]");
    assert_rejects("[1,\f2]");
    assert_rejects("{\"a\"\v:1}");
    assert_rejects("\xc2\xa0[1]");     // no-break space
    assert_rejects("\xef\xbb\xbf[1]"); // BOM is not part of the text

    assert_rejects("");
    assert_rejects(" \r\n\t ");
    BB_ASSERT(bb_json_parse(NULL) == NULL);
}

void test_parse_malformed_documents(void)
{
    printf("\tTesting malformed JSON documents are rejected...\n");

    char *invalid[] = {
        // literals
        "nullx", "null0", "truee", "falsee", "True", "TRUE", "NULL", "False", "nul",
        "[nul]", "[truefalse]", "[null null]", "tru e", "n", "t", "f", "undefined",
        "[truex]", "{\"a\":nulll}",
        // structure
        "[,]", "[,1]", "{,}", "{\"a\":}", "{\"a\"}", "[1 2]", "[1,,2]", "}", "]", "[", "{",
        ",", ":", "1 2", "\"a\" \"b\"", "{}{}", "[][]", "[1]]", "{\"a\":1}}", "[}", "{]",
        "{\"a\":1,\"b\"}", "{\"a\" \"b\"}", "{\"a\"::1}", "{1:2}", "{null:1}", "{[1]:2}",
        "{\"a\":1 \"b\":2}", "[\"a\" \"b\"]", "[[1],]", "{\"a\":{},}",
        // non-standard extensions
        "/* c */ 1", "// c\n1", "[1] // c", "[1, /* c */ 2]", "{a: 1}", "[1,2,3,]",
        "[undefined]", "[NaN]", "[Infinity]", "[-Infinity]",
        NULL
    };
    for (int i = 0; invalid[i] != NULL; i++)
        assert_rejects(invalid[i]);
}

void test_object_key_escaping(void)
{
    printf("\tTesting JSON object keys with escapes...\n");

    bb_json_t *obj = assert_accepts(
        "{\"a\\\"b\": 1, \"c\\\\d\": 2, \"\\u0041\": 3, \"tab\\t\": 4, \"\": 5,"
        " \"\\u00e9\": 6, \"\\ud83d\\ude00\": 7, \"sl\\/ash\": 8}");
    BB_ASSERT(bb_json_get_size(obj) == 8);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(obj, "a\"b")) == 1);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(obj, "c\\d")) == 2);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(obj, "A")) == 3);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(obj, "tab\t")) == 4);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(obj, "")) == 5);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(obj, "\xc3\xa9")) == 6);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(obj, "\xf0\x9f\x98\x80")) == 7);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(obj, "sl/ash")) == 8);
    BB_ASSERT(bb_json_object_get_value(obj, "a\\\"b") == NULL);

    // Whatever was parsed must serialize back to JSON that parses to the same value.
    char *buffer = NULL;
    int size = 0;
    BB_ASSERT(!BB_FAILED(bb_json_serialize(obj, &buffer, &size)));
    bb_json_t *again = assert_accepts(buffer);
    BB_ASSERT(bb_json_equal(obj, again));
    free(buffer);
    bb_json_destroy(again);

    buffer = NULL;
    BB_ASSERT(!BB_FAILED(bb_json_serialize_indented(obj, &buffer, &size)));
    BB_ASSERT(size == (int)strlen(buffer));
    again = assert_accepts(buffer);
    BB_ASSERT(bb_json_equal(obj, again));
    free(buffer);
    bb_json_destroy(again);
    bb_json_destroy(obj);

    // Keys that need escaping are escaped on output.
    obj = bb_json_new_object();
    bb_json_object_set_value(obj, "quo\"te", bb_json_new_int(1));
    bb_json_object_set_value(obj, "back\\slash", bb_json_new_int(2));
    bb_json_object_set_value(obj, "new\nline", bb_json_new_int(3));
    bb_json_object_set_value(obj, "ctl\x01", bb_json_new_int(4));
    bb_json_object_set_value(obj, "caf\xc3\xa9", bb_json_new_int(5));
    assert_serializes_to(obj,
        "{\"quo\\\"te\": 1, \"back\\\\slash\": 2, \"new\\nline\": 3, \"ctl\\u0001\": 4, \"caf\xc3\xa9\": 5}");
    bb_json_destroy(obj);

    char *invalid[] = {
        "{\"a\nb\":1}", "{\"a\\xb\":1}", "{\"a\\\":1}", "{\"abc", "{\"abc\"", "{\"abc\":",
        "{\"a\\u12\":1}", "{\"\\ud83d\":1}", "{\"\\u0000\":1}", "{'a':1}", NULL
    };
    for (int i = 0; invalid[i] != NULL; i++)
        assert_rejects(invalid[i]);
}

void test_serialize_utf8_passthrough(void)
{
    printf("\tTesting serializing non-ASCII text...\n");

    // Bytes >= 0x80 (negative as plain char) must not be mistaken for control characters.
    const char *utf8 = "caf\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80 del\x7f";
    bb_json_t *json = bb_json_new_text(utf8);
    char expected[64];
    snprintf(expected, sizeof(expected), "\"%s\"", utf8);
    assert_serializes_to(json, expected);

    char *buffer = NULL;
    int size = 0;
    bb_json_serialize(json, &buffer, &size);
    bb_json_t *parsed = assert_accepts(buffer);
    BB_ASSERT(bb_json_equal(json, parsed));
    free(buffer);
    bb_json_destroy(parsed);

    // \u escapes and the raw characters are the same string once parsed.
    bb_json_t *escaped = assert_accepts("\"caf\\u00e9 \\u20ac \\ud83d\\ude00 del\\u007f\"");
    BB_ASSERT(bb_json_equal(json, escaped));
    bb_json_destroy(escaped);
    bb_json_destroy(json);
}

void test_serialize_real_valid_json(void)
{
    printf("\tTesting serializing JSON reals as valid JSON numbers...\n");

    struct { float value; const char *expected; } exact[] = {
        { 1.0f, "1.0" },       { -1.0f, "-1.0" },     { 0.0f, "0.0" },
        { -0.0f, "-0.0" },     { 0.5f, "0.5" },       { -0.5f, "-0.5" },
        { 100.0f, "100.0" },   { 2.5f, "2.5" },       { 11.47f, "11.47" },
        { 0.1f, "0.1" },       { 123456.0f, "123456.0" }, { 0.00001f, "0.00001" },
        { 37.7668f, "37.7668" }, { -122.3959f, "-122.3959" },
    };
    for (size_t i = 0; i < sizeof(exact) / sizeof(exact[0]); i++)
    {
        bb_json_t *json = bb_json_new_real(exact[i].value);
        assert_serializes_to(json, exact[i].expected);
        bb_json_destroy(json);
    }

    // Every real must come back as a REAL with the identical value.
    float values[] = {
        1.0f, 3.14159274f, 16777216.0f, 0.3f, 1234567936.0f, -2.5e-7f, 1e-10f, 1e30f,
        FLT_MAX, -FLT_MAX, FLT_MIN, 1.4e-45f, 1e9f, 999999.9f, 0.000123f, 7e-6f, 12345.678f,
    };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
    {
        bb_json_t *json = bb_json_new_real(values[i]);
        char *buffer = NULL;
        int size = 0;
        BB_ASSERT(!BB_FAILED(bb_json_serialize(json, &buffer, &size)));
        BB_ASSERT(size == (int)strlen(buffer));
        bb_json_t *parsed = parse_exact(buffer);
        if (parsed == NULL)
            fprintf(stderr, "Serialized real is not valid JSON: %s\n", buffer);
        BB_ASSERT(parsed != NULL);
        BB_ASSERT(bb_json_get_type(parsed) == BB_JSON_REAL);
        BB_ASSERT(bb_json_get_value_real(parsed) == values[i]);
        free(buffer);
        bb_json_destroy(parsed);
        bb_json_destroy(json);
    }

    // NaN and infinities have no JSON form; they are written as null.
    float specials[] = { strtof("nan", NULL), INFINITY, -INFINITY };
    for (size_t i = 0; i < sizeof(specials) / sizeof(specials[0]); i++)
    {
        bb_json_t *json = bb_json_new_real(specials[i]);
        assert_serializes_to(json, "null");
        bb_json_destroy(json);
    }

    // Reals survive a parse -> serialize -> parse cycle without turning into ints
    // or into invalid JSON such as "1.".
    bb_json_t *doc = assert_accepts("[3.0, 2.50, 10.0, 0.0, 1e2, 1.5e-3]");
    char *buffer = NULL;
    int size = 0;
    BB_ASSERT(!BB_FAILED(bb_json_serialize(doc, &buffer, &size)));
    bb_json_t *again = assert_accepts(buffer);
    BB_ASSERT(bb_json_equal(doc, again));
    for (unsigned int i = 0; i < 6; i++)
        BB_ASSERT(bb_json_get_type(bb_json_array_get_index(again, i)) == BB_JSON_REAL);
    free(buffer);
    bb_json_destroy(again);
    bb_json_destroy(doc);

    doc = assert_accepts("[3.0, 2.5, 10.0]");
    assert_serializes_to(doc, "[3.0, 2.5, 10.0]");
    bb_json_destroy(doc);
}

void test_serialize_integer_extremes(void)
{
    printf("\tTesting serializing extreme JSON integers...\n");

    bb_json_t *json = bb_json_new_int(INT_MIN);
    assert_serializes_to(json, "-2147483648");
    bb_json_destroy(json);

    json = bb_json_new_int(INT_MAX);
    assert_serializes_to(json, "2147483647");
    bb_json_destroy(json);

    json = bb_json_new_int(0);
    assert_serializes_to(json, "0");
    bb_json_destroy(json);

    json = bb_json_new_int(-10);
    assert_serializes_to(json, "-10");
    bb_json_destroy(json);

    json = bb_json_new_int(1000000);
    assert_serializes_to(json, "1000000");
    bb_json_destroy(json);
}

void test_serialize_invalid_arguments(void)
{
    printf("\tTesting serializing with invalid arguments...\n");

    char *buffer = NULL;
    int size = 0;
    BB_ASSERT(BB_FAILED(bb_json_serialize(NULL, &buffer, &size)));
    BB_ASSERT(buffer == NULL);
    BB_ASSERT(BB_FAILED(bb_json_serialize_indented(NULL, &buffer, &size)));
    BB_ASSERT(buffer == NULL);

    bb_json_t *json = bb_json_new_int(1);
    BB_ASSERT(BB_FAILED(bb_json_serialize(json, &buffer, NULL)));
    BB_ASSERT(BB_FAILED(bb_json_serialize_indented(json, &buffer, NULL)));
    bb_json_destroy(json);
}

void test_parse_nesting_depth_limit(void)
{
    printf("\tTesting JSON nesting depth limit...\n");

    // Hostile input must be rejected, not overflow the stack.
    const size_t depth = 200000;
    char *buffer = malloc(depth * 6 + 8);
    BB_ASSERT(buffer != NULL);

    // Unterminated arrays
    memset(buffer, '[', depth);
    buffer[depth] = '\0';
    BB_ASSERT(bb_json_parse(buffer) == NULL);

    // Balanced arrays
    memset(buffer, '[', depth);
    buffer[depth] = '0';
    memset(buffer + depth + 1, ']', depth);
    buffer[depth * 2 + 1] = '\0';
    BB_ASSERT(bb_json_parse(buffer) == NULL);

    // Nested objects
    size_t pos = 0;
    for (size_t i = 0; i < depth; i++)
    {
        memcpy(buffer + pos, "{\"a\":", 5);
        pos += 5;
    }
    buffer[pos++] = '0';
    for (size_t i = 0; i < depth; i++)
        buffer[pos++] = '}';
    buffer[pos] = '\0';
    BB_ASSERT(bb_json_parse(buffer) == NULL);
    free(buffer);

    // Reasonable nesting of mixed containers still works.
    const int levels = 200;
    buffer = malloc((size_t)levels * 12 + 8);
    BB_ASSERT(buffer != NULL);
    pos = 0;
    for (int i = 0; i < levels; i++)
    {
        if (i % 2 == 0)
            buffer[pos++] = '[';
        else
        {
            memcpy(buffer + pos, "{\"k\":", 5);
            pos += 5;
        }
    }
    buffer[pos++] = '1';
    for (int i = levels - 1; i >= 0; i--)
        buffer[pos++] = (i % 2 == 0) ? ']' : '}';
    buffer[pos] = '\0';
    bb_json_t *json = bb_json_parse(buffer);
    BB_ASSERT(json != NULL);
    bb_json_destroy(json);
    free(buffer);
}

void test_parse_many_strings(void)
{
    printf("\tTesting parsing arrays with many strings...\n");

    // Each string must be parsed in time proportional to its own length, not to the
    // length of everything that follows it in the document.
    const int count = 30000;
    size_t cap = (size_t)count * 16 + 16;
    char *buffer = malloc(cap);
    BB_ASSERT(buffer != NULL);
    size_t pos = 0;
    buffer[pos++] = '[';
    for (int i = 0; i < count; i++)
        pos += (size_t)snprintf(buffer + pos, cap - pos, "%s\"s%d\"", i ? "," : "", i);
    buffer[pos++] = ']';
    buffer[pos] = '\0';

    bb_json_t *json = bb_json_parse(buffer);
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_size(json) == (size_t)count);
    BB_ASSERT(strcmp(bb_json_get_value_text(bb_json_array_get_index(json, 0)), "s0") == 0);
    BB_ASSERT(strcmp(bb_json_get_value_text(bb_json_array_get_index(json, count - 1)), "s29999") == 0);
    bb_json_destroy(json);
    free(buffer);
}

static void write_binary_file(const char *path, const char *data, size_t len)
{
    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);
    BB_ASSERT(fwrite(data, 1, len, f) == len);
    fclose(f);
}

void test_json_load_edge_cases(void)
{
    printf("\tTesting JSON load of unusual files...\n");

    const char *path = "bb_json_load_edge_case.json";

    // UTF-8 byte order mark is skipped.
    write_binary_file(path, "\xef\xbb\xbf{\"a\": 1}", 3 + 8);
    bb_json_t *json = bb_json_load(path);
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(json, "a")) == 1);
    bb_json_destroy(json);

    // An embedded NUL must not silently truncate the document.
    write_binary_file(path, "[1, 2]\0garbage", 14);
    BB_ASSERT(bb_json_load(path) == NULL);

    // Trailing garbage, empty and whitespace-only files are invalid.
    write_binary_file(path, "[1, 2] x", 8);
    BB_ASSERT(bb_json_load(path) == NULL);
    write_binary_file(path, "", 0);
    BB_ASSERT(bb_json_load(path) == NULL);
    write_binary_file(path, " \n", 2);
    BB_ASSERT(bb_json_load(path) == NULL);

    // Windows line endings are plain whitespace.
    write_binary_file(path, "{\r\n  \"a\": [1,\r\n 2]\r\n}\r\n", 23);
    json = bb_json_load(path);
    BB_ASSERT(json != NULL);
    BB_ASSERT(bb_json_get_size(bb_json_object_get_value(json, "a")) == 2);
    bb_json_destroy(json);

    remove(path);
}

int main(void)
{
    printf("Running JSON tests...\n");
    test_json_text();
    test_json_array();
    test_bb_json_array_remove_at_index();
    test_json_array_multi_remove_at_index();
    test_json_array_remove_boundaries();
    test_json_array_remove_empty_and_reuse();
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

    test_parse_invalid_numbers();
    test_parse_integer_boundaries();
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
    test_compare_jsons_different_structure();

    test_json_deep_nesting();
    test_json_clone();
    test_json_clone_independence();
    test_json_clone_array_independence();
    test_json_object_merge();
    test_json_object_merge_independence();
    test_json_object_merge_invalid_type();

    test_json_control_characters_roundtrip();

    // Standard JSON (RFC 8259) conformance
    test_parse_valid_standard_documents();
    test_parse_exponent_numbers();
    test_parse_number_range_and_forms();
    test_parse_unicode_escapes();
    test_parse_string_escape_validation();
    test_parse_rejects_raw_control_characters();
    test_parse_whitespace_rules();
    test_parse_malformed_documents();
    test_parse_nesting_depth_limit();
    test_parse_many_strings();
    test_object_key_escaping();
    test_serialize_utf8_passthrough();
    test_serialize_real_valid_json();
    test_serialize_integer_extremes();
    test_serialize_invalid_arguments();
    test_json_load_edge_cases();

    test_dump_and_bb_json_load();
    test_json_load_missing_file();
    test_serialize_indented_roundtrip();

    test_json_dsl_macros();
    printf("All tests passed.\n");
    return 0;
}
