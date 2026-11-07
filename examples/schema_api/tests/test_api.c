/* End-to-end test of the schema_api handlers over real HTTP, on one backend.
 *
 *   test_api <json|sqlite|postgres> <port>
 *
 * The same assertions run on every backend, so this is also the check that the
 * backends agree: same validation, same filtering, same bytewise sort order,
 * same stable paging. The server runs in-process; requests go through the
 * framework's own HTTP client.
 *
 * postgres needs a build with BB_WITH_POSTGRES and BB_TEST_POSTGRES_URI set;
 * without them the run is skipped. All data is tagged with a per-run marker and
 * removed again, so a shared database can be reused. */

#include "api.h"

#include "Book_schema.generated.h"
#include "Note_schema.generated.h"

#include <blue-bird/error/assert.h>
#include <blue-bird/persist/model/model_json.h>
#include <blue-bird/persist/model/model_sqlite.h>
#ifdef BB_WITH_POSTGRES
#include <blue-bird/persist/model/model_postgres.h>
#endif
#include <blue-bird/runtime/runtime.h>
#include <blue-bird/utils/json.h>
#include <blue-bird/utils/platform.h>
#include <blue-bird/utils/uuid.h>
#include <blue-bird/web/client.h>
#include <blue-bird/web/server.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_port;
static bb_runtime_t *g_runtime;
static const bb_model_api_t *g_model;
static bb_model_handle_t *g_handle;

/* ---------------------------------------------------------------------------
 * Server thread
 * ------------------------------------------------------------------------- */

static void *server_thread(void *arg)
{
    (void)arg;
    g_runtime = bb_runtime_create();

    bb_server_t *server = bb_server_create_on_runtime(g_runtime, g_port);
    api_add_routes(server);
    bb_server_start(server);

    bb_runtime_run(g_runtime);

    bb_server_destroy(server);
    bb_runtime_destroy(g_runtime);
    return NULL;
}

/* ---------------------------------------------------------------------------
 * HTTP and JSON helpers
 * ------------------------------------------------------------------------- */

/* Sends a request; returns the status. `json` (optional) receives the parsed
 * body, or NULL if the body was empty or not JSON. The caller destroys it. */
static int call(const char *method, const char *path, const char *body, bb_json_t **json)
{
    char url[1024];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d%s", g_port, path);

    bb_client_t *client = bb_client_create();
    bb_request_t *req = bb_client_get_request(client);
    bb_response_t *res = bb_client_get_response(client);

    bb_request_set_method(req, method);
    bb_request_set_url(req, url);
    bb_request_set_body(req, (char *)(body ? body : "")); /* copied, not modified */

    bb_error_t err = bb_client_execute(client);
    BB_ASSERT(err.code == 0); /* an answer arrived */

    int status = bb_response_get_status(res);

    if (json)
    {
        *json = NULL;
        const char *text = bb_response_get_body(res);
        if (text && *text)
        {
            char *copy = bb_strdup(text);
            *json = bb_json_parse(copy);
            free(copy);
        }
    }

    bb_client_destroy(client);
    return status;
}

static size_t array_len(bb_json_t *array)
{
    BB_ASSERT(array && bb_json_get_type(array) == BB_JSON_ARRAY);
    size_t n = 0;
    while (bb_json_array_get_index(array, (unsigned int)n))
        n++;
    return n;
}

static bb_json_t *item(bb_json_t *array, size_t i)
{
    bb_json_t *e = bb_json_array_get_index(array, (unsigned int)i);
    BB_ASSERT(e);
    return e;
}

static const char *text_of(bb_json_t *obj, const char *key)
{
    bb_json_t *v = bb_json_object_get_value(obj, key);
    BB_ASSERT(v && bb_json_get_type(v) == BB_JSON_TEXT);
    return bb_json_get_value_text(v);
}

static int int_of(bb_json_t *obj, const char *key)
{
    bb_json_t *v = bb_json_object_get_value(obj, key);
    BB_ASSERT(v && bb_json_get_type(v) == BB_JSON_INT);
    return bb_json_get_value_integer(v);
}

/* A status plus an {"error": "..."} body */
static void expect_error(const char *method, const char *path, const char *body, int status)
{
    bb_json_t *json = NULL;
    BB_ASSERT(call(method, path, body, &json) == status);
    BB_ASSERT(json);
    BB_ASSERT(strlen(text_of(json, "error")) > 0);
    bb_json_destroy(json);
}

/* ---------------------------------------------------------------------------
 * Test data
 * ------------------------------------------------------------------------- */

#define MAX_IDS 32

static char g_marker[16];
static char g_book_ids[MAX_IDS][BB_UUID_BUF_LEN];
static size_t g_book_count;
static char g_note_id[BB_UUID_BUF_LEN];

/* Creates a book with the given author tag, remembers its id, returns it. */
static const char *create_book(const char *author_suffix, const char *title, int year)
{
    char body[512];
    snprintf(body, sizeof(body),
             "{\"title\":\"%s\",\"author\":\"%s-%s\",\"year\":%d}",
             title, g_marker, author_suffix, year);

    bb_json_t *json = NULL;
    BB_ASSERT(call("POST", "/books", body, &json) == 201);
    BB_ASSERT(json);

    const char *id = text_of(json, "id");
    BB_ASSERT(strlen(id) == 36);
    BB_ASSERT(g_book_count < MAX_IDS);
    snprintf(g_book_ids[g_book_count], BB_UUID_BUF_LEN, "%s", id);

    BB_ASSERT(strcmp(text_of(json, "title"), title) == 0);
    BB_ASSERT(int_of(json, "year") == year);
    bb_json_destroy(json);

    return g_book_ids[g_book_count++];
}

/* GET /books?author=<marker>-<suffix>&<extra>, returns the parsed array */
static bb_json_t *list_books(const char *author_suffix, const char *extra)
{
    char path[512];
    snprintf(path, sizeof(path), "/books?author=%s-%s%s%s",
             g_marker, author_suffix, extra && *extra ? "&" : "", extra ? extra : "");

    bb_json_t *json = NULL;
    BB_ASSERT(call("GET", path, NULL, &json) == 200);
    BB_ASSERT(json);
    return json;
}

static void expect_titles(bb_json_t *array, const char *const *titles, size_t n)
{
    BB_ASSERT(array_len(array) == n);
    for (size_t i = 0; i < n; i++)
        BB_ASSERT(strcmp(text_of(item(array, i), "title"), titles[i]) == 0);
}

static void expect_years(bb_json_t *array, const int *years, size_t n)
{
    BB_ASSERT(array_len(array) == n);
    for (size_t i = 0; i < n; i++)
        BB_ASSERT(int_of(item(array, i), "year") == years[i]);
}

/* ---------------------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------------------- */

static void test_describe(void)
{
    printf("\tGET / describes the registered schemas...\n");

    bb_json_t *json = NULL;
    BB_ASSERT(call("GET", "/", NULL, &json) == 200);
    BB_ASSERT(json);

    bb_json_t *resources = bb_json_object_get_value(json, "resources");
    BB_ASSERT(resources && array_len(resources) >= 2);

    int books = 0, notes = 0;
    for (size_t i = 0; i < array_len(resources); i++)
    {
        bb_json_t *r = item(resources, i);
        const char *name = text_of(r, "name");

        char expected_path[64];
        snprintf(expected_path, sizeof(expected_path), "/%s", name);
        BB_ASSERT(strcmp(text_of(r, "path"), expected_path) == 0);

        if (strcmp(name, "books") == 0)
        {
            books = 1;
            bb_json_t *fields = bb_json_object_get_value(r, "fields");
            BB_ASSERT(fields && array_len(fields) == 4);

            bb_json_t *id = item(fields, 0);
            BB_ASSERT(strcmp(text_of(id, "name"), "id") == 0);
            BB_ASSERT(strcmp(text_of(id, "type"), "uuid") == 0);

            bb_json_t *title = item(fields, 1);
            BB_ASSERT(strcmp(text_of(title, "type"), "string") == 0);
            BB_ASSERT(int_of(title, "max_bytes") == 127);
        }
        if (strcmp(name, "notes") == 0)
            notes = 1;
    }
    BB_ASSERT(books && notes);
    bb_json_destroy(json);
}

static void test_create_and_validation(void)
{
    printf("\tPOST /:resource creates, validates and ignores a client-chosen id...\n");

    /* the server generates the id even if the client sends one */
    char body[256];
    snprintf(body, sizeof(body),
             "{\"id\":\"hacker\",\"title\":\"Probe\",\"author\":\"%s-P\",\"year\":1}", g_marker);

    bb_json_t *json = NULL;
    BB_ASSERT(call("POST", "/books", body, &json) == 201);
    BB_ASSERT(json);
    const char *id = text_of(json, "id");
    BB_ASSERT(strcmp(id, "hacker") != 0 && strlen(id) == 36);
    snprintf(g_book_ids[g_book_count++], BB_UUID_BUF_LEN, "%s", id);
    bb_json_destroy(json);

    char author[64];
    snprintf(author, sizeof(author), "%s-X", g_marker);

    char missing[256], wrong_type[256], empty[256];
    snprintf(missing, sizeof(missing), "{\"title\":\"No author\"}");
    snprintf(wrong_type, sizeof(wrong_type), "{\"title\":\"t\",\"author\":\"%s\",\"year\":\"1999\"}", author);
    snprintf(empty, sizeof(empty), "{\"title\":\"\",\"author\":\"%s\"}", author);

    expect_error("POST", "/books", missing, 400);          /* missing required field   */
    expect_error("POST", "/books", wrong_type, 400);       /* year must be an integer  */
    expect_error("POST", "/books", empty, 400);            /* required, but empty      */
    expect_error("POST", "/books", "{nope", 400);          /* not JSON                 */
    expect_error("POST", "/books", "[1,2]", 400);          /* JSON, but not an object  */
    expect_error("POST", "/books", "", 400);               /* no body                  */

    /* title field is 128 bytes: 127 fit, 128 do not */
    char long_title[129], fits[512], too_long[512];
    memset(long_title, 'a', 127);
    long_title[127] = '\0';
    snprintf(fits, sizeof(fits), "{\"title\":\"%s\",\"author\":\"%s\"}", long_title, author);
    long_title[127] = 'a';
    long_title[128] = '\0';
    snprintf(too_long, sizeof(too_long), "{\"title\":\"%s\",\"author\":\"%s\"}", long_title, author);

    expect_error("POST", "/books", too_long, 400);

    BB_ASSERT(call("POST", "/books", fits, &json) == 201);
    BB_ASSERT(json && strlen(text_of(json, "title")) == 127);
    snprintf(g_book_ids[g_book_count++], BB_UUID_BUF_LEN, "%s", text_of(json, "id"));
    bb_json_destroy(json);

    /* unknown resources */
    expect_error("GET", "/widgets", NULL, 404);
    expect_error("POST", "/widgets", "{}", 404);
}

static void test_second_resource(void)
{
    printf("\tA second schema is served by the same handlers...\n");

    char body[256];
    snprintf(body, sizeof(body), "{\"title\":\"%s\",\"body\":\"hello\",\"pinned\":1}", g_marker);

    bb_json_t *json = NULL;
    BB_ASSERT(call("POST", "/notes", body, &json) == 201);
    BB_ASSERT(json);
    snprintf(g_note_id, sizeof(g_note_id), "%s", text_of(json, "id"));
    bb_json_destroy(json);

    char path[128];
    snprintf(path, sizeof(path), "/notes/%s", g_note_id);
    BB_ASSERT(call("GET", path, NULL, &json) == 200);
    BB_ASSERT(json);
    BB_ASSERT(strcmp(text_of(json, "body"), "hello") == 0);
    BB_ASSERT(int_of(json, "pinned") == 1);
    bb_json_destroy(json);

    snprintf(path, sizeof(path), "/notes?title=%s", g_marker);
    BB_ASSERT(call("GET", path, NULL, &json) == 200);
    BB_ASSERT(json && array_len(json) == 1);
    bb_json_destroy(json);
}

static void test_list_filter_sort_page(void)
{
    printf("\tGET /:resource filters, sorts and pages...\n");

    /* author A: titles chosen so bytewise order differs from locale order */
    create_book("A", "apple", 1971);
    create_book("A", "Banana", 1965);
    create_book("A", "cherry", 1980);
    /* author B */
    create_book("B", "Dune", 1965);
    create_book("B", "Emma", 1815);

    bb_json_t *list;

    list = list_books("A", NULL);
    BB_ASSERT(array_len(list) == 3);
    bb_json_destroy(list);

    /* bytewise: uppercase sorts before lowercase on every backend */
    static const char *const asc[] = { "Banana", "apple", "cherry" };
    static const char *const desc[] = { "cherry", "apple", "Banana" };
    list = list_books("A", "sort=title");
    expect_titles(list, asc, 3);
    bb_json_destroy(list);
    list = list_books("A", "sort=title&order=desc");
    expect_titles(list, desc, 3);
    bb_json_destroy(list);

    static const int years_desc[] = { 1980, 1971, 1965 };
    list = list_books("A", "sort=year&order=desc");
    expect_years(list, years_desc, 3);
    bb_json_destroy(list);

    /* paging */
    static const int page1[] = { 1965, 1971 }, page2[] = { 1980 };
    list = list_books("A", "sort=year&limit=2&offset=0");
    expect_years(list, page1, 2);
    bb_json_destroy(list);
    list = list_books("A", "sort=year&limit=2&offset=2");
    expect_years(list, page2, 1);
    bb_json_destroy(list);
    list = list_books("A", "sort=year&limit=2&offset=3");
    BB_ASSERT(array_len(list) == 0);
    bb_json_destroy(list);

    /* filter on an int field, combined with another filter */
    static const char *const dune[] = { "Dune" };
    list = list_books("B", "year=1965");
    expect_titles(list, dune, 1);
    bb_json_destroy(list);

    list = list_books("nobody", NULL);
    BB_ASSERT(array_len(list) == 0);
    bb_json_destroy(list);

    /* bad query parameters */
    char path[256];
    snprintf(path, sizeof(path), "/books?author=%s-A&year=abc", g_marker);
    expect_error("GET", path, NULL, 400);
    snprintf(path, sizeof(path), "/books?author=%s-A&sort=nope", g_marker);
    expect_error("GET", path, NULL, 400);
    snprintf(path, sizeof(path), "/books?author=%s-A&limit=0", g_marker);
    expect_error("GET", path, NULL, 400);
    snprintf(path, sizeof(path), "/books?author=%s-A&order=sideways", g_marker);
    expect_error("GET", path, NULL, 400);
}

/* Many rows share a sort key; pages must still partition the result exactly:
 * no row twice, none missing. That is what the primary-key tie-break is for. */
static void test_stable_paging_with_ties(void)
{
    printf("\tPaging is stable when rows tie on the sort key...\n");

    enum { N = 5 };
    char created[N][BB_UUID_BUF_LEN];

    for (int i = 0; i < N; i++)
    {
        char title[16];
        snprintf(title, sizeof(title), "tie%d", i);
        snprintf(created[i], BB_UUID_BUF_LEN, "%s", create_book("C", title, 2000));
    }

    int seen[N] = { 0 };
    int total = 0;

    for (int offset = 0; offset < N + 2; offset += 2)
    {
        char extra[64];
        snprintf(extra, sizeof(extra), "sort=year&limit=2&offset=%d", offset);

        bb_json_t *page = list_books("C", extra);
        for (size_t i = 0; i < array_len(page); i++)
        {
            const char *id = text_of(item(page, i), "id");
            int found = 0;
            for (int k = 0; k < N; k++)
                if (strcmp(created[k], id) == 0)
                {
                    seen[k]++;
                    found = 1;
                }
            BB_ASSERT(found);
            total++;
        }
        bb_json_destroy(page);
    }

    BB_ASSERT(total == N);
    for (int k = 0; k < N; k++)
        BB_ASSERT(seen[k] == 1);
}

static void test_get_replace_delete(void)
{
    printf("\tGET, PUT and DELETE /:resource/:id...\n");

    const char *id = g_book_ids[g_book_count - 1]; /* a "tie" book, year 2000 */
    char path[128], body[512];
    snprintf(path, sizeof(path), "/books/%s", id);

    bb_json_t *json = NULL;
    BB_ASSERT(call("GET", path, NULL, &json) == 200);
    BB_ASSERT(json && strcmp(text_of(json, "id"), id) == 0);
    BB_ASSERT(int_of(json, "year") == 2000);
    bb_json_destroy(json);

    expect_error("GET", "/books/not-a-real-id", NULL, 404);

    /* PUT replaces: the key comes from the URL, an omitted optional field resets */
    snprintf(body, sizeof(body), "{\"id\":\"ignored\",\"title\":\"Replaced\",\"author\":\"%s-C\"}", g_marker);
    BB_ASSERT(call("PUT", path, body, &json) == 200);
    BB_ASSERT(json);
    BB_ASSERT(strcmp(text_of(json, "id"), id) == 0);
    BB_ASSERT(strcmp(text_of(json, "title"), "Replaced") == 0);
    BB_ASSERT(int_of(json, "year") == 0);
    bb_json_destroy(json);

    BB_ASSERT(call("GET", path, NULL, &json) == 200);
    BB_ASSERT(json && strcmp(text_of(json, "title"), "Replaced") == 0 && int_of(json, "year") == 0);
    bb_json_destroy(json);

    expect_error("PUT", path, "{\"title\":\"only a title\"}", 400);
    expect_error("PUT", "/books/00000000-0000-4000-8000-000000000000", body, 404);

    BB_ASSERT(call("DELETE", path, NULL, NULL) == 204);
    expect_error("DELETE", path, NULL, 404);
    expect_error("GET", path, NULL, 404);
    g_book_count--; /* already gone: not part of the cleanup */
}

static void test_cleanup(void)
{
    printf("\tRemoving everything this run created...\n");

    char path[128];
    for (size_t i = 0; i < g_book_count; i++)
    {
        snprintf(path, sizeof(path), "/books/%s", g_book_ids[i]);
        BB_ASSERT(call("DELETE", path, NULL, NULL) == 204);
    }
    snprintf(path, sizeof(path), "/notes/%s", g_note_id);
    BB_ASSERT(call("DELETE", path, NULL, NULL) == 204);

    const char *suffixes[] = { "A", "B", "C", "P", "X" };
    for (size_t i = 0; i < sizeof(suffixes) / sizeof(suffixes[0]); i++)
    {
        bb_json_t *list = list_books(suffixes[i], NULL);
        BB_ASSERT(array_len(list) == 0);
        bb_json_destroy(list);
    }
}

/* ---------------------------------------------------------------------------
 * Backend selection and main
 * ------------------------------------------------------------------------- */

static int open_backend(const char *which, const char *file)
{
    if (strcmp(which, "json") == 0)
    {
        g_model = bb_model_json_api();
        g_handle = g_model->open(file);
    }
    else if (strcmp(which, "sqlite") == 0)
    {
        g_model = bb_model_sqlite_api();
        g_handle = g_model->open(file);
    }
    else if (strcmp(which, "postgres") == 0)
    {
#ifdef BB_WITH_POSTGRES
        const char *uri = getenv("BB_TEST_POSTGRES_URI");
        if (!uri || !*uri)
        {
            printf("Skipping schema_api tests on postgres (BB_TEST_POSTGRES_URI not set)\n");
            exit(0);
        }
        g_model = bb_model_postgres_api();
        g_handle = g_model->open(uri);
#else
        printf("Skipping schema_api tests on postgres (built without BB_WITH_POSTGRES)\n");
        exit(0);
#endif
    }
    else
    {
        fprintf(stderr, "usage: test_api <json|sqlite|postgres> <port>\n");
        return -1;
    }

    return g_handle ? 0 : -1;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: test_api <json|sqlite|postgres> <port>\n");
        return 2;
    }

    const char *which = argv[1];
    g_port = atoi(argv[2]);

    char file[128];
    snprintf(file, sizeof(file), "schema_api_test_%d.%s", g_port, strcmp(which, "json") == 0 ? "json" : "db");
    remove(file);

    BB_ASSERT(open_backend(which, file) == 0);
    BB_ASSERT(api_register(&Book_schema, g_model, g_handle) == 0);
    BB_ASSERT(api_register(&Note_schema, g_model, g_handle) == 0);

    bb_uuid_t uuid;
    BB_ASSERT(bb_uuid_v4_string(uuid) == 0);
    snprintf(g_marker, sizeof(g_marker), "t%.8s", uuid);

    pthread_t thread_id;
    BB_ASSERT(pthread_create(&thread_id, NULL, server_thread, NULL) == 0);
    while (g_runtime == NULL || !bb_runtime_is_running(g_runtime))
        bb_usleep(10000);

    printf("Running schema_api tests on %s...\n", which);

    test_describe();
    test_create_and_validation();
    test_second_resource();
    test_list_filter_sort_page();
    test_stable_paging_with_ties();
    test_get_replace_delete();
    test_cleanup();

    printf("All schema_api tests passed on %s\n", which);

    bb_runtime_stop(g_runtime);
    pthread_join(thread_id, NULL);

    g_model->close(g_handle);
    remove(file);
    bb_runtime_destroy(bb_runtime_default());
    return 0;
}
