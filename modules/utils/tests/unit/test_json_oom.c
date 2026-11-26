// Out-of-memory tests for json.c.
//
// json.c is compiled into this test directly, with its allocation calls routed
// through wrappers that can be told to fail the Nth allocation. Every scenario is
// then run repeatedly, failing allocation 1, then 2, then 3 and so on, until a run
// completes without hitting a failure. After each injected failure the scenarios
// check that the API reported it, that no data was corrupted, and that objects
// still work; AddressSanitizer/LeakSanitizer (when enabled) catch leaks and
// double frees on the cleanup paths.
//
// Nothing here changes the normal build: only json.c's own calls are redirected.

#include <blue-bird/error/assert.h>
#include <blue-bird/utils/platform.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <math.h>
#include <errno.h>

static size_t g_oom_fail_at = 0;  // 1-based index of the allocation to fail, 0 = never
static size_t g_oom_count = 0;
static bool g_oom_hit = false;
static int g_oom_paused = 0;  // nesting depth; failures are only injected at depth 0

static bool oom_should_fail(void)
{
    if (g_oom_paused > 0 || g_oom_fail_at == 0)
        return false;
    if (++g_oom_count == g_oom_fail_at)
    {
        g_oom_hit = true;
        return true;
    }
    return false;
}

static void *oom_malloc(size_t size)
{
    return oom_should_fail() ? NULL : malloc(size);
}

static void *oom_calloc(size_t count, size_t size)
{
    return oom_should_fail() ? NULL : calloc(count, size);
}

static void *oom_realloc(void *ptr, size_t size)
{
    // A failed realloc leaves the original block untouched, as in the C standard.
    return oom_should_fail() ? NULL : realloc(ptr, size);
}

static char *oom_strdup(const char *s)
{
    return oom_should_fail() ? NULL : bb_strdup(s);
}

#define malloc oom_malloc
#define calloc oom_calloc
#define realloc oom_realloc
#define bb_strdup oom_strdup
#include "../../src/json.c"
#undef malloc
#undef calloc
#undef realloc
#undef bb_strdup

static void oom_pause(void)  { g_oom_paused++; }
static void oom_resume(void) { BB_ASSERT(g_oom_paused > 0); g_oom_paused--; }

// Runs the scenario once per allocation it makes, failing a different one each time.
static void oom_sweep(const char *name, void (*scenario)(void))
{
    printf("\tTesting JSON out-of-memory handling: %s...\n", name);
    size_t injected = 0;
    for (size_t n = 1; n <= 200000; n++)
    {
        g_oom_fail_at = n;
        g_oom_count = 0;
        g_oom_hit = false;
        g_oom_paused = 0;
        scenario();
        g_oom_fail_at = 0;
        BB_ASSERT(g_oom_paused == 0);
        if (!g_oom_hit)
        {
            // The scenario ran to the end without needing allocation number n, so
            // every allocation it makes has been failed once. It must make some.
            BB_ASSERT(injected > 0);
            printf("\t\t(%zu allocation failures injected)\n", injected);
            return;
        }
        injected++;
    }
    BB_ASSERT(false && "scenario never finished; allocation count keeps growing");
}

static bb_json_t *parse_paused(const char *text)
{
    oom_pause();
    char *copy = bb_strdup(text);
    BB_ASSERT(copy != NULL);
    bb_json_t *json = bb_json_parse(copy);
    free(copy);
    oom_resume();
    return json;
}

static char *serialize_paused(bb_json_t *json, bool indented)
{
    oom_pause();
    char *buffer = NULL;
    int size = 0;
    bb_error_t err = indented ? bb_json_serialize_indented(json, &buffer, &size)
                              : bb_json_serialize(json, &buffer, &size);
    BB_ASSERT(!BB_FAILED(err));
    oom_resume();
    return buffer;
}

// ---------------------------------------------------------------------------------

#define ARRAY_ELEMENTS 40

static void scenario_array_push(void)
{
    bb_json_t *arr = bb_json_create(BB_JSON_ARRAY);
    if (!arr)
        return;

    int expected[ARRAY_ELEMENTS];
    size_t pushed = 0;
    for (int i = 0; i < ARRAY_ELEMENTS; i++)
    {
        bb_json_t *value = bb_json_new_int(i);
        if (!value)
            continue;
        size_t before = bb_json_get_size(arr);
        if (BB_FAILED(bb_json_array_push(arr, value)))
        {
            // The caller still owns the element, and the array is unchanged.
            bb_json_destroy(value);
            BB_ASSERT(bb_json_get_size(arr) == before);
        }
        else
        {
            expected[pushed++] = i;
            BB_ASSERT(bb_json_get_size(arr) == before + 1);
        }
    }

    oom_pause();
    BB_ASSERT(bb_json_get_size(arr) == pushed);
    for (size_t k = 0; k < pushed; k++)
        BB_ASSERT(bb_json_get_value_integer(bb_json_array_get_index(arr, (unsigned int)k)) == expected[k]);
    BB_ASSERT(bb_json_array_get_index(arr, (unsigned int)pushed) == NULL);

    // Removal after failed growth still leaves a consistent array.
    if (pushed > 0)
    {
        BB_ASSERT(!BB_FAILED(bb_json_array_remove_at_index(arr, 0)));
        BB_ASSERT(bb_json_get_size(arr) == pushed - 1);
    }
    oom_resume();
    bb_json_destroy(arr);
}

#define OBJECT_KEYS 40

static void format_key(char *out, size_t size, int i)
{
    snprintf(out, size, "key%d", i);
}

static void scenario_object_insert(void)
{
    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);
    if (!obj)
        return;

    bool present[OBJECT_KEYS] = { false };
    size_t count = 0;
    for (int i = 0; i < OBJECT_KEYS; i++)
    {
        char key[16];
        format_key(key, sizeof(key), i);
        bb_json_t *value = bb_json_new_int(i);
        if (!value)
            continue;
        size_t before = bb_json_get_size(obj);
        if (BB_FAILED(bb_json_object_set_value(obj, key, value)))
        {
            bb_json_destroy(value);
            BB_ASSERT(bb_json_get_size(obj) == before);
            oom_pause();
            BB_ASSERT(bb_json_object_get_value(obj, key) == NULL);
            oom_resume();
        }
        else
        {
            present[i] = true;
            count++;
            BB_ASSERT(bb_json_get_size(obj) == before + 1);
        }
    }

    // Overwrite every key that made it in; a failed overwrite must keep the old value.
    int expected_value[OBJECT_KEYS];
    for (int i = 0; i < OBJECT_KEYS; i++)
    {
        expected_value[i] = i;
        if (!present[i])
            continue;
        char key[16];
        format_key(key, sizeof(key), i);
        bb_json_t *value = bb_json_new_int(i + 1000);
        if (!value)
            continue;
        if (BB_FAILED(bb_json_object_set_value(obj, key, value)))
            bb_json_destroy(value);
        else
            expected_value[i] = i + 1000;
    }

    oom_pause();
    BB_ASSERT(bb_json_get_size(obj) == count);
    char expected[4096] = "{";
    size_t seen = 0;
    for (int i = 0; i < OBJECT_KEYS; i++)
    {
        char key[16];
        format_key(key, sizeof(key), i);
        bb_json_t *value = bb_json_object_get_value(obj, key);
        if (!present[i])
        {
            BB_ASSERT(value == NULL);
            continue;
        }
        BB_ASSERT(value != NULL);
        BB_ASSERT(bb_json_get_value_integer(value) == expected_value[i]);
        char entry[48];
        snprintf(entry, sizeof(entry), "%s\"%s\": %d", seen ? ", " : "", key, expected_value[i]);
        strcat(expected, entry);
        seen++;
    }
    strcat(expected, "}");
    char *serialized = serialize_paused(obj, false);
    BB_ASSERT(strcmp(serialized, expected) == 0);
    free(serialized);

    // Every key that is there can be removed again.
    for (int i = 0; i < OBJECT_KEYS; i++)
    {
        char key[16];
        format_key(key, sizeof(key), i);
        if (present[i])
            BB_ASSERT(!BB_FAILED(bb_json_object_remove_key(obj, key)));
    }
    BB_ASSERT(bb_json_get_size(obj) == 0);
    oom_resume();
    bb_json_destroy(obj);
}

static void scenario_text_setter(void)
{
    bb_json_t *text = bb_json_new_text("initial value");
    if (!text)
        return;

    const char *values[] = { "short", "a considerably longer replacement text", "", "x" };
    char expected[64] = "initial value";
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++)
    {
        bb_error_t err = bb_json_set_value_text(text, values[i]);
        if (BB_FAILED(err))
        {
            // A failed assignment keeps the previous text, not a dangling or empty one.
            BB_ASSERT(err.code == BB_ERR_ALLOC);
        }
        else
        {
            snprintf(expected, sizeof(expected), "%s", values[i]);
        }
        BB_ASSERT(bb_json_get_value_text(text) != NULL);
        BB_ASSERT(strcmp(bb_json_get_value_text(text), expected) == 0);
        BB_ASSERT(bb_json_get_size(text) == strlen(expected));
    }
    bb_json_destroy(text);
}

static const char *const SAMPLE_DOC =
    "{\"name\": \"Bob \\u00e9\\n\", \"list\": [1, 2.5, true, null, \"x\", [], {}], "
    "\"nested\": {\"a\": {\"b\": {\"c\": [\"deep\", {\"k\": 7}]}}}, "
    "\"k1\": 1, \"k2\": 2, \"k3\": 3, \"k4\": 4, \"k5\": 5, \"k6\": 6, \"k7\": 7, \"k8\": 8}";

static void scenario_clone(void)
{
    bb_json_t *src = parse_paused(SAMPLE_DOC);
    BB_ASSERT(src != NULL);

    bb_json_t *copy = bb_json_clone(src);
    if (copy)
    {
        BB_ASSERT(copy != src);
        BB_ASSERT(bb_json_equal(src, copy));
        bb_json_destroy(copy);
    }

    // The source is untouched whether or not the clone worked.
    bb_json_t *reference = parse_paused(SAMPLE_DOC);
    BB_ASSERT(reference != NULL && bb_json_equal(src, reference));
    bb_json_destroy(reference);
    bb_json_destroy(src);
}

static void scenario_merge(void)
{
    bb_json_t *dst = parse_paused("{\"a\": 1, \"k2\": \"old\", \"z\": [1, 2]}");
    bb_json_t *src = parse_paused(SAMPLE_DOC);
    BB_ASSERT(dst != NULL && src != NULL);

    bb_error_t err = bb_json_object_merge(dst, src);

    oom_pause();
    bb_json_t *reference = parse_paused(SAMPLE_DOC);
    BB_ASSERT(reference != NULL);
    // src is never modified by a merge, successful or not.
    BB_ASSERT(bb_json_equal(src, reference));

    if (!BB_FAILED(err))
    {
        // Everything from src is now in dst, with src's values winning.
        for (_bb_hash_table_node_t *node = src->object.order_head; node; node = node->order_next)
            BB_ASSERT(bb_json_equal(bb_json_object_get_value(dst, node->key), node->value));
        // Keys that only dst has are untouched.
        BB_ASSERT(bb_json_get_size(bb_json_object_get_value(dst, "z")) == 2);
    }
    else
    {
        BB_ASSERT(err.code == BB_ERR_ALLOC);
    }

    // dst is a usable, serializable object either way, and untouched keys survive.
    char *serialized = serialize_paused(dst, true);
    BB_ASSERT(serialized != NULL);
    free(serialized);
    BB_ASSERT(bb_json_object_get_value(dst, "a") != NULL);
    oom_resume();

    bb_json_destroy(reference);
    bb_json_destroy(dst);
    bb_json_destroy(src);
}

static char *g_reference_compact = NULL;
static char *g_reference_indented = NULL;

static void scenario_serialize(void)
{
    bb_json_t *doc = parse_paused(SAMPLE_DOC);
    BB_ASSERT(doc != NULL);

    char *compact = NULL;
    int size = 0;
    bb_error_t err = bb_json_serialize(doc, &compact, &size);
    if (BB_FAILED(err))
    {
        BB_ASSERT(err.code == BB_ERR_ALLOC);
        BB_ASSERT(compact == NULL);
    }
    else
    {
        BB_ASSERT(strcmp(compact, g_reference_compact) == 0);
        BB_ASSERT((size_t)size == strlen(compact));
        free(compact);
    }

    // The indented variant replaces a buffer the caller passes in; on failure that
    // buffer must be left alone, on success it is released and replaced.
    oom_pause();
    char *buffer = bb_strdup("sentinel");
    oom_resume();
    BB_ASSERT(buffer != NULL);
    err = bb_json_serialize_indented(doc, &buffer, &size);
    if (BB_FAILED(err))
    {
        BB_ASSERT(err.code == BB_ERR_ALLOC);
        BB_ASSERT(strcmp(buffer, "sentinel") == 0);
    }
    else
    {
        BB_ASSERT(strcmp(buffer, g_reference_indented) == 0);
        BB_ASSERT((size_t)size == strlen(buffer));
    }
    free(buffer);
    bb_json_destroy(doc);
}

static char g_parse_input[1024];

static void scenario_parse(void)
{
    // g_parse_input holds a document with escapes, nesting, many keys and a number
    // longer than the parser's stack buffer, so every allocation site in the parser runs.
    bb_json_t *reference = parse_paused(g_parse_input);
    BB_ASSERT(reference != NULL);

    char *copy = (char *)malloc(strlen(g_parse_input) + 1);
    BB_ASSERT(copy != NULL);
    memcpy(copy, g_parse_input, strlen(g_parse_input) + 1);
    bb_json_t *parsed = bb_json_parse(copy);
    if (parsed)
    {
        BB_ASSERT(bb_json_equal(parsed, reference));
        bb_json_destroy(parsed);
    }
    BB_ASSERT(strcmp(copy, g_parse_input) == 0);
    free(copy);
    bb_json_destroy(reference);
}

static void scenario_load_and_dump(void)
{
    const char *path = "bb_json_oom_test.json";
    bb_json_t *first = parse_paused("{\"version\": 1, \"items\": [1, 2, 3]}");
    bb_json_t *second = parse_paused(SAMPLE_DOC);
    BB_ASSERT(first != NULL && second != NULL);

    oom_pause();
    BB_ASSERT(!BB_FAILED(bb_json_dump(first, path)));
    oom_resume();

    // A failed dump leaves the earlier file alone; a successful one replaces it.
    bb_error_t err = bb_json_dump(second, path);
    oom_pause();
    bb_json_t *on_disk = bb_json_load(path);
    BB_ASSERT(on_disk != NULL);
    BB_ASSERT(bb_json_equal(on_disk, BB_FAILED(err) ? first : second));
    bb_json_destroy(on_disk);
    oom_resume();

    // Loading either returns the whole document or nothing.
    bb_json_t *loaded = bb_json_load(path);
    if (loaded)
    {
        oom_pause();
        BB_ASSERT(bb_json_equal(loaded, BB_FAILED(err) ? first : second));
        oom_resume();
        bb_json_destroy(loaded);
    }

    bb_json_destroy(first);
    bb_json_destroy(second);
    remove(path);
}

static void scenario_create_all_types(void)
{
    bb_json_type_t types[] = {
        BB_JSON_BOOL, BB_JSON_INT, BB_JSON_REAL, BB_JSON_TEXT, BB_JSON_NULL, BB_JSON_ARRAY, BB_JSON_OBJECT,
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
    {
        bb_json_t *node = bb_json_create(types[i]);
        if (node)
        {
            BB_ASSERT(bb_json_get_type(node) == types[i]);
            bb_json_destroy(node);
        }
    }
}

int main(void)
{
    oom_pause();
    bb_json_t *doc = parse_paused(SAMPLE_DOC);
    BB_ASSERT(doc != NULL);
    g_reference_compact = serialize_paused(doc, false);
    g_reference_indented = serialize_paused(doc, true);
    bb_json_destroy(doc);

    // A long fractional number (past the parser's 128-byte stack buffer), escaped text,
    // and enough keys to force the object to grow.
    char digits[160];
    memset(digits, '3', sizeof(digits));
    digits[sizeof(digits) - 1] = '\0';
    snprintf(g_parse_input, sizeof(g_parse_input),
             "{\"long\": [0.%s], \"text\": \"tab\\tquote\\\" \\u00e9\\ud83d\\ude00\", "
             "\"k1\": 1, \"k2\": 2, \"k3\": 3, \"k4\": 4, \"k5\": 5, \"k6\": 6, \"k7\": 7, "
             "\"nest\": [[{\"a\": [true, null]}]]}",
             digits);
    oom_resume();

    oom_sweep("array growth", scenario_array_push);
    oom_sweep("object insert, resize and overwrite", scenario_object_insert);
    oom_sweep("text setter", scenario_text_setter);
    oom_sweep("clone", scenario_clone);
    oom_sweep("merge", scenario_merge);
    oom_sweep("serialize", scenario_serialize);
    oom_sweep("parse", scenario_parse);
    oom_sweep("load and dump", scenario_load_and_dump);
    oom_sweep("create", scenario_create_all_types);

    free(g_reference_compact);
    free(g_reference_indented);
    printf("All tests passed.\n");
    return 0;
}
