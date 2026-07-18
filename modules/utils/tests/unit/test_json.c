#include "blue-bird/utils/json.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void test_json_text(void)
{
    printf("\tTesting JSON text...\n");
    bb_json_t *json = bb_json_create(BB_JSON_TEXT);
    bb_json_set_value_text(json, "Hello there!");
    assert(bb_json_get_size(json) == 12);
    assert(strcmp(bb_json_get_value_text(json), "Hello there!") == 0);
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
    assert(bb_json_get_size(arr) == 5);
    for (unsigned int i = 0; i < bb_json_get_size(arr); i++)
    {
        assert(strcmp(bb_json_get_value_text(bb_json_array_get_index(arr, i)), vals[i]) == 0);
    }
    bb_json_destroy(arr);
}

void test_bb_json_array_remove_at_index(void)
{
    printf("\tTesting JSON array remove at index...\n");
    bb_json_t *arr = bb_json_parse("[1, 2, 3, 4, 5, 6, 7, 8]");
    bb_json_array_remove_at_index(arr, 2);
    assert(bb_json_get_size(arr) == 7);
    char *buffer;
    int size;
    bb_json_serialize(arr, &buffer, &size);
    assert(strcmp(buffer, "[1, 2, 4, 5, 6, 7, 8]") == 0);
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
        bb_json_array_remove_at_index(arr, bb_json_get_size(arr) - 1);
    }
    for (int i = 19; i > 0; i -= 2)
    {
        bb_json_array_remove_at_index(arr, i);
    }
    char *buffer;
    int size;
    bb_json_serialize(arr, &buffer, &size);
    assert(strcmp(buffer, "[0, 2, 4, 6, 8, 10, 12, 14, 16, 18]") == 0);
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
        assert(strcmp(bb_json_get_value_text(res), vals[i]) == 0);
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
    assert(bb_json_get_value_integer(res) == 2);

    bb_json_destroy(obj);
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
    assert(res == NULL);

    bb_json_destroy(obj);
}

void test_serialize_text_json(void)
{
    printf("\tTesting serializing JSON text...\n");
    bb_json_t *json = bb_json_create(BB_JSON_TEXT);
    bb_json_set_value_text(json, "123456");
    char *buffer;
    int size;
    bb_json_serialize(json, &buffer, &size);
    assert(strcmp(buffer, "\"123456\"") == 0);
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
    assert(strcmp(buffer, "\"sample text:\\t12\\\\34\\nanother line.\"") == 0);
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
    assert(strcmp(buffer, "[\"ZERO\", \"ONE\", \"TWO\", \"THREE\", \"FOUR\"]") == 0);
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
    assert(strcmp(buffer, "{\"one\": \"ichi\", \"two\": \"nii\", \"three\": \"san\", \"four\": \"yon\"}") == 0);
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
        sprintf(key, "%d", i);
        bb_json_t *value = bb_json_create(BB_JSON_ARRAY);
        for (int j = 0; j < i; j++)
        {
            bb_json_t *child = bb_json_create(BB_JSON_INT);
            bb_json_set_value_integer(child, j);
            bb_json_array_push(value, child);
        }
        bb_json_object_set_value(json, key, value);
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
    bb_json_serialize(json, &serialize_buffer, &size);

    assert(strcmp(large_buffer, serialize_buffer) == 0);
    free(serialize_buffer);
    free(large_buffer);
    bb_json_destroy(json);
}

void test_parse_and_bb_json_serialize(void)
{
    printf("\tTesting JSON parsing...\n");
    char *s = "[\"one\", \"two\", {\"some thing\": null, \"other thing\": false, \"and the other thing\": [1, 2, 3, 11.47]}, null, [\"first\", 15, true, false]]";
    bb_json_t *json = bb_json_parse(s);

    char *buffer;
    int size;
    bb_json_serialize(json, &buffer, &size);
    assert(strcmp(buffer, s) == 0);
    free(buffer);
    bb_json_destroy(json);
}

void test_parse_empty_text_json(void)
{
    printf("\tTesting empty text JSON parsing...\n");
    bb_json_t *json = bb_json_parse("\"\"");
    assert(json != NULL);
    assert(bb_json_get_size(json) == 0);
    bb_json_destroy(json);
}

void test_parse_empty_array_json(void)
{
    printf("\tTesting empty array JSON parsing...\n");
    bb_json_t *json = bb_json_parse("[]");
    assert(json != NULL);
    assert(bb_json_get_size(json) == 0);
    bb_json_destroy(json);
}

void test_parse_empty_object_json(void)
{
    printf("\tTesting empty object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{}");
    assert(json != NULL);
    assert(bb_json_get_size(json) == 0);
    bb_json_destroy(json);
}

void test_parse_large_json(void)
{
    printf("\tTesting large JSON parsing...\n");
    unsigned int n = 100;
    char *large_buffer = (char *)malloc(sizeof(char) * 20000);
    int index = sprintf(large_buffer, "{");
    for (unsigned int i = 0; i < n; i++)
    {
        index += sprintf(large_buffer + index, "\"%d\": ", i);
        index += sprintf(large_buffer + index, "[");
        for (unsigned int j = 0; j < i; j++)
            index += sprintf(large_buffer + index, "%d%s", j, (j + 1) < i ? ", " : "");
        index += sprintf(large_buffer + index, "]%s", (i + 1) < n ? ", " : "");
    }
    index += sprintf(large_buffer + index, "}");
    bb_json_t *json = bb_json_parse(large_buffer);
    assert(bb_json_get_size(json) == n);
    for (unsigned int i = 0; i < n; i++)
    {
        char key[4];
        sprintf(key, "%d", i);
        bb_json_t *child = bb_json_object_get_value(json, key);
        assert(child);
        assert(bb_json_get_type(child) == BB_JSON_ARRAY);
        assert(bb_json_get_size(child) == i);
        for (unsigned int j = 0; j < i; j++)
        {
            bb_json_t *sub_child = bb_json_array_get_index(child, j);
            assert(bb_json_get_type(sub_child) == BB_JSON_INT);
            assert(bb_json_get_value_integer(sub_child) == (int)j);
        }
    }
    bb_json_destroy(json);
}

void test_parse_text_with_escapes(void)
{
    printf("\tTesting parse JSON text with escapes...\n");

    // JSON source text (escaped, not raw C escapes)
    char *s = "\"hello\\nworld\\t\\u0001\"";

    bb_json_t *json = bb_json_parse(s);
    assert(json != NULL);

    assert(bb_json_get_type(json) == BB_JSON_TEXT);

    // "hello\nworld\t\x01"
    // indexes: 0123456789012
    assert(bb_json_get_size(json) == 13);

    char *text_val = bb_json_get_value_text(json);
    assert(text_val[5] == '\n');
    assert(text_val[11] == '\t');
    assert((unsigned char)text_val[12] == 0x01);

    char *buf;
    int size;
    bb_json_serialize(json, &buf, &size);
    assert(strcmp(buf, "\"hello\\nworld\\t\\u0001\"") == 0);
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
    str_size = strlen(buffer);
    assert(size == str_size);
    assert(null_size == str_size);
    free(buffer);
    bb_json_destroy(json);
}

// Broken JSON Parsing:
void test_incomplete_text_json(void)
{
    printf("\tTesting incomplete text JSON parsing...\n");
    bb_json_t *json = bb_json_parse("\"text with no ending quotation mark.");
    assert(json == NULL);
}

void test_incomplete_array_json(void)
{
    printf("\tTesting incomplete array JSON parsing...\n");
    bb_json_t *json = bb_json_parse("[1, 2, 3");
    assert(json == NULL);
}

void test_multiple_comma_array_json(void)
{
    printf("\tTesting multiple comma array JSON parsing...\n");
    bb_json_t *json = bb_json_parse("[1, 2, , 3]");
    assert(json == NULL);
}

void test_missing_comma_array_json(void)
{
    printf("\tTesting missing comma array JSON parsing...\n");
    bb_json_t *json = bb_json_parse("[1, 2 3, 4]");
    assert(json == NULL);
}

void test_incomplete_object_json(void)
{
    printf("\tTesting incomplete object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3");
    assert(json == NULL);
}

void test_multiple_comma_object_json(void)
{
    printf("\tTesting multiple comma object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, , \"two\": 2, \"three\": 3}");
    assert(json == NULL);
}

void test_missing_comma_object_json(void)
{
    printf("\tTesting missing comma object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1 \"two\": 2, \"three\": 3}");
    assert(json == NULL);
}

void test_missing_colon_object_json(void)
{
    printf("\tTesting missing colon object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, \"two\" 2, \"three\": 3}");
    assert(json == NULL);
}

void test_missing_value_object_json(void)
{
    printf("\tTesting missing value object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, \"two\": , \"three\": 3}");
    assert(json == NULL);
}

void test_missing_key_object_json(void)
{
    printf("\tTesting missing key object JSON parsing...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, : 2 , \"three\": 3}");
    assert(json == NULL);
}

void test_parse_json_with_trailing_str(void)
{
    printf("\tTesting parsing object JSON with trailing str...\n");
    bb_json_t *json = bb_json_parse("{\"one\": 1, \"two\": 2 , \"three\": 3}something unrelated");
    assert(json == NULL);
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
    int size;
    bb_json_serialize(json, &buffer, &size);
    assert(strcasecmp(buffer, "[0, 1, 2, 3]") == 0);
    free(buffer);
}

void test_compare_equal_jsons(void)
{
    printf("\tTesting comparison of equal JSONs...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    assert(bb_json_compare(json_1, json_2) == 0);
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_compare_equal_jsons_different_order(void)
{
    printf("\tTesting comparison of equal JSONs with different order...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"three\": 3, \"two\": 2}");
    assert(bb_json_compare(json_1, json_2) == 0);
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_compare_jsons_missing_key(void)
{
    printf("\tTesting comparison of JSONs: missing key...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"two\": 2}");
    assert(bb_json_compare(json_1, json_2) != 0);
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_compare_jsons_extra_key(void)
{
    printf("\tTesting comparison of JSONs: extra key...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3}");
    assert(bb_json_compare(json_1, json_2) != 0);
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_compare_complex_equal_jsons(void)
{
    printf("\tTesting comparison of complex equal JSONs...\n");
    bb_json_t *json_1 = bb_json_parse("{\"one\": 1, \"two\": 2, \"three\": 3, \"list\": [1, \"two\", 3.14, null, true, false, {\"name\": \"Alice\", \"age\": 30}]}");
    bb_json_t *json_2 = bb_json_parse("{\"one\": 1, \"three\": 3, \"list\": [1, \"two\", 3.14, null, true, false, {\"name\": \"Alice\", \"age\": 30}], \"two\": 2}");
    assert(bb_json_compare(json_1, json_2) == 0);
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
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
    assert(!BB_FAILED(bb_json_dump(json_1, "test_file.json")));
    bb_json_t *json_2 = bb_json_load("test_file.json");
    char *buf;
    int size;
    bb_json_serialize(json_2, &buf, &size);

    assert(bb_json_get_type(json_1) == bb_json_get_type(json_2));
    assert(bb_json_get_size(json_1) == bb_json_get_size(json_2));
    for (unsigned int i = 0; i < bb_json_get_size(json_1); i++)
        assert(bb_json_get_value_integer(bb_json_array_get_index(json_1, i)) == bb_json_get_value_integer(bb_json_array_get_index(json_2, i)));
    
    bb_json_destroy(json_1);
    bb_json_destroy(json_2);
}

void test_json_dsl_macros(void)
{
    printf("\tTesting JSON DSL Macros...\n");
    bb_json_t *doc = BB_JSON(
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
    bb_json_serialize(doc, &buf, &size);
    assert(strcmp(buf, "{\"name\": \"Alice\", \"age\": 30, \"admin\": false, \"scores\": [10, 20, 30], \"misc\": null, \"nested\": {\"pi\": 3.14, \"ok\": true}}") == 0);
    bb_json_destroy(doc);
}

int main(void)
{
    printf("Running JSON tests...\n");
    test_json_text();
    test_json_array();
    test_bb_json_array_remove_at_index();
    test_json_array_multi_remove_at_index();
    test_json_object();
    test_object_key_overwrite();
    test_object_key_deletion();

    test_serialize_text_json();
    test_serialize_array_json();
    test_serialize_object_json();
    test_serialize_large_json();

    test_parse_and_bb_json_serialize();
    test_parse_empty_text_json();
    test_parse_empty_array_json();
    test_parse_empty_object_json();
    test_parse_large_json();
    test_parse_text_with_escapes();
    test_serialize_json_size();

    test_incomplete_text_json();
    test_incomplete_array_json();
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

    test_dump_and_bb_json_load();

    test_json_dsl_macros();
    printf("All tests passed.\n");
    return 0;
}
