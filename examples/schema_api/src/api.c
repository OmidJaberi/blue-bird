#include "api.h"

#include <blue-bird/persist/query.h>
#include <blue-bird/persist/repo.h>
#include <blue-bird/persist/serialization/entity_json.h>
#include <blue-bird/utils/json.h>
#include <blue-bird/utils/uuid.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Schemas as data.
 *
 * Nothing in this file mentions Book, Note or any other entity. Every handler
 * works from a bb_schema_t -- field names, types, sizes, offsets and flags --
 * so one set of handlers serves every schema that is registered.
 * ------------------------------------------------------------------------- */

#define API_MAX_RESOURCES 16
#define API_DEFAULT_LIMIT 50
#define API_MAX_LIMIT     200

typedef struct {
    bb_schema_t *schema;
    bb_repo_t repo;
} api_resource_t;

static api_resource_t g_resources[API_MAX_RESOURCES];
static size_t g_resource_count;

int api_register(bb_schema_t *schema, const bb_model_api_t *model, bb_model_handle_t *handle)
{
    if (g_resource_count >= API_MAX_RESOURCES)
        return -1;

    /* The framework's schema registry validates the schema and lets the
     * handlers find it again by name at request time. */
    if (bb_schema_register(schema) != 0)
        return -1;

    api_resource_t *r = &g_resources[g_resource_count++];
    r->schema = schema;
    bb_repo_init(&r->repo, model, handle, schema);
    return 0;
}

static api_resource_t *find_resource(const char *name)
{
    bb_schema_t *schema = bb_schema_get(name);
    if (!schema)
        return NULL;

    for (size_t i = 0; i < g_resource_count; i++)
        if (g_resources[i].schema == schema)
            return &g_resources[i];

    return NULL;
}

static bb_field_t *pk_field(bb_schema_t *schema)
{
    return &schema->fields[schema->primary_key_index];
}

/* ---------------------------------------------------------------------------
 * Replies. Errors are reported as JSON bodies with a status code, and the
 * handlers return success once they have set one. (A handler that returns an
 * error gets its status sent too, but skips the post-middleware.)
 * ------------------------------------------------------------------------- */

static bb_error_t reply_json(bb_response_t *res, int status, bb_json_t *json)
{
    char *buf = NULL;
    int size = 0;

    bb_error_t err = bb_json_serialize(json, &buf, &size);
    bb_json_destroy(json);

    if (BB_FAILED(err) || !buf)
    {
        free(buf);
        bb_response_set_status(res, 500);
        return BB_SUCCESS();
    }

    bb_response_set_status(res, status);
    bb_response_set_header(res, "Content-Type", "application/json");
    bb_response_set_body(res, buf);
    free(buf);
    return BB_SUCCESS();
}

static bb_error_t reply_error(bb_response_t *res, int status, const char *fmt, ...)
{
    char message[256];

    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    bb_json_t *obj = bb_json_new_object();
    bb_json_object_set_value(obj, "error", bb_json_new_text(message));
    return reply_json(res, status, obj);
}

/* ---------------------------------------------------------------------------
 * Keys and filter values arrive as text and must become the typed value the
 * repo expects, which depends on the field's type.
 * ------------------------------------------------------------------------- */

static int parse_int(const char *text, int *out)
{
    if (!text || !*text)
        return -1;

    char *end = NULL;
    errno = 0;
    long v = strtol(text, &end, 10);

    if (errno != 0 || *end != '\0' || v < INT_MIN || v > INT_MAX)
        return -1;

    *out = (int)v;
    return 0;
}

/* Turns the :id path segment into the typed primary key the repo expects.
 * `number` provides storage for integer keys. */
static int parse_key(const bb_field_t *pk, const char *text, int *number, const void **key)
{
    if (!text || !*text)
        return -1;

    switch (pk->type)
    {
        case BB_FIELD_INT:
            if (parse_int(text, number) != 0)
                return -1;
            *key = number;
            return 0;

        case BB_FIELD_STRING:
        case BB_FIELD_UUID:
            if (strlen(text) >= pk->size)
                return -1;
            *key = text;
            return 0;

        default:
            return -1;
    }
}

static void set_key(const bb_field_t *pk, void *entity, const void *key)
{
    char *dst = (char *)entity + pk->offset;

    if (pk->type == BB_FIELD_INT)
        *(int *)dst = *(const int *)key;
    else
        strncpy(dst, (const char *)key, pk->size - 1); /* entity is zeroed */
}

/* ---------------------------------------------------------------------------
 * Request bodies.
 *
 * bb_json_to_entity() silently truncates a string that does not fit its
 * field. For a persistence API that would store a shortened value without
 * telling the client, so every value is checked against the schema first and
 * over-long input is rejected instead.
 * ------------------------------------------------------------------------- */

static bb_json_t *parse_body(bb_request_t *req)
{
    const char *body = bb_request_get_body(req);
    if (!body || !*body)
        return NULL;

    char *copy = malloc(strlen(body) + 1);
    if (!copy)
        return NULL;
    strcpy(copy, body);

    bb_json_t *json = bb_json_parse(copy);
    free(copy);

    if (json && bb_json_get_type(json) != BB_JSON_OBJECT)
    {
        bb_json_destroy(json);
        json = NULL;
    }
    return json;
}

/* Fields the server fills in itself. Whatever the client sent for them is
 * dropped: a generated UUID on create, the key from the URL on replace. */
static int is_server_owned(const bb_schema_t *schema, size_t index, int creating)
{
    const bb_field_t *f = &schema->fields[index];

    if (creating)
        return (f->flags & BB_FIELD_AUTO_GENERATE) && f->type == BB_FIELD_UUID;

    return index == schema->primary_key_index;
}

/* Checks `json` against the schema's types, sizes and REQUIRED flags.
 * Returns 0, or -1 after writing a message into `err`. */
static int validate_body(bb_schema_t *schema, bb_json_t *json, int creating,
                         char *err, size_t err_size)
{
    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];

        if (f->type == BB_FIELD_BLOB)
            continue; /* not supported over JSON */

        if (is_server_owned(schema, i, creating))
        {
            bb_json_object_remove_key(json, f->name);
            continue;
        }

        bb_json_t *val = bb_json_object_get_value(json, f->name);
        int required = (f->flags & (BB_FIELD_REQUIRED | BB_FIELD_PRIMARY_KEY)) != 0;

        if (!val)
        {
            if (required)
            {
                snprintf(err, err_size, "missing required field '%s'", f->name);
                return -1;
            }
            continue;
        }

        if (f->type == BB_FIELD_INT)
        {
            if (bb_json_get_type(val) != BB_JSON_INT)
            {
                snprintf(err, err_size, "field '%s' must be an integer", f->name);
                return -1;
            }
            continue;
        }

        /* STRING and UUID */
        if (bb_json_get_type(val) != BB_JSON_TEXT)
        {
            snprintf(err, err_size, "field '%s' must be a string", f->name);
            return -1;
        }

        const char *text = bb_json_get_value_text(val);
        size_t len = text ? strlen(text) : 0;

        if (len >= f->size)
        {
            snprintf(err, err_size, "field '%s' is too long (at most %zu bytes)",
                     f->name, f->size - 1);
            return -1;
        }
        if (required && len == 0)
        {
            snprintf(err, err_size, "field '%s' must not be empty", f->name);
            return -1;
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * Handlers
 * ------------------------------------------------------------------------- */

static const char *type_name(bb_field_type_t type)
{
    switch (type)
    {
        case BB_FIELD_INT:    return "int";
        case BB_FIELD_STRING: return "string";
        case BB_FIELD_UUID:   return "uuid";
        case BB_FIELD_BLOB:   return "blob";
        default:              return "unknown";
    }
}

/* GET / -- the API describes itself from the same metadata that drives it. */
bb_error_t api_describe(bb_request_t *req, bb_response_t *res)
{
    (void)req;

    bb_json_t *resources = bb_json_new_array();

    for (size_t i = 0; i < g_resource_count; i++)
    {
        bb_schema_t *schema = g_resources[i].schema;
        bb_json_t *fields = bb_json_new_array();

        for (size_t j = 0; j < schema->field_count; j++)
        {
            bb_field_t *f = &schema->fields[j];
            bb_json_t *field = bb_json_new_object();

            bb_json_object_set_value(field, "name", bb_json_new_text(f->name));
            bb_json_object_set_value(field, "type", bb_json_new_text(type_name(f->type)));

            if (f->type == BB_FIELD_STRING)
                bb_json_object_set_value(field, "max_bytes", bb_json_new_int((int)f->size - 1));
            if (j == schema->primary_key_index)
                bb_json_object_set_value(field, "primary_key", bb_json_new_bool(true));
            if (f->flags & BB_FIELD_REQUIRED)
                bb_json_object_set_value(field, "required", bb_json_new_bool(true));
            if (f->flags & BB_FIELD_AUTO_GENERATE)
                bb_json_object_set_value(field, "auto_generate", bb_json_new_bool(true));

            bb_json_array_push(fields, field);
        }

        char path[128];
        snprintf(path, sizeof(path), "/%s", schema->name);

        bb_json_t *resource = bb_json_new_object();
        bb_json_object_set_value(resource, "name", bb_json_new_text(schema->name));
        bb_json_object_set_value(resource, "path", bb_json_new_text(path));
        bb_json_object_set_value(resource, "fields", fields);
        bb_json_array_push(resources, resource);
    }

    bb_json_t *root = bb_json_new_object();
    bb_json_object_set_value(root, "resources", resources);
    return reply_json(res, 200, root);
}

/* Builds a bb_query_t from the URL: ?<field>=<value> filters on equality for
 * any field of the schema, plus sort, order, limit and offset. The filters
 * are found by walking the schema's fields, not by knowing the entity. */
static int build_query(api_resource_t *r, bb_request_t *req, bb_query_t *q,
                       int numbers[BB_QUERY_MAX_CONDITIONS], char *err, size_t err_size)
{
    bb_schema_t *schema = r->schema;
    size_t conditions = 0;

    bb_query_init(q, schema);

    for (size_t i = 0; i < schema->field_count; i++)
    {
        bb_field_t *f = &schema->fields[i];
        const char *text = bb_request_get_query_param(req, f->name);

        if (!text || f->type == BB_FIELD_BLOB)
            continue;

        if (conditions >= BB_QUERY_MAX_CONDITIONS)
        {
            snprintf(err, err_size, "too many filters (at most %d)", BB_QUERY_MAX_CONDITIONS);
            return -1;
        }

        const void *value = text;
        if (f->type == BB_FIELD_INT)
        {
            if (parse_int(text, &numbers[conditions]) != 0)
            {
                snprintf(err, err_size, "filter '%s' must be an integer", f->name);
                return -1;
            }
            value = &numbers[conditions];
        }

        bb_query_where(q, f->name, BB_OP_EQ, value);
        conditions++;
    }

    /* Sort by the requested field, then by primary key, so that paging
     * returns a stable order on every backend. */
    bb_field_t *pk = pk_field(schema);
    bb_field_t *sort_field = NULL;
    bb_query_order_t direction = BB_ORDER_ASC;

    const char *order = bb_request_get_query_param(req, "order");
    if (order)
    {
        if (strcmp(order, "desc") == 0)
            direction = BB_ORDER_DESC;
        else if (strcmp(order, "asc") != 0)
        {
            snprintf(err, err_size, "order must be 'asc' or 'desc'");
            return -1;
        }
    }

    const char *sort = bb_request_get_query_param(req, "sort");
    if (sort)
    {
        sort_field = bb_schema_find_field(schema, sort);
        if (!sort_field || sort_field->type == BB_FIELD_BLOB)
        {
            snprintf(err, err_size, "cannot sort by '%s'", sort);
            return -1;
        }
        bb_query_order_by(q, sort_field->name, direction);
    }
    if (sort_field != pk)
        bb_query_order_by(q, pk->name, BB_ORDER_ASC);

    int limit = API_DEFAULT_LIMIT;
    const char *limit_text = bb_request_get_query_param(req, "limit");
    if (limit_text && (parse_int(limit_text, &limit) != 0 || limit < 1))
    {
        snprintf(err, err_size, "limit must be a positive integer");
        return -1;
    }
    if (limit > API_MAX_LIMIT)
        limit = API_MAX_LIMIT;

    int offset = 0;
    const char *offset_text = bb_request_get_query_param(req, "offset");
    if (offset_text && (parse_int(offset_text, &offset) != 0 || offset < 0))
    {
        snprintf(err, err_size, "offset must be a non-negative integer");
        return -1;
    }

    bb_query_limit(q, limit);
    bb_query_offset(q, offset);
    return 0;
}

/* GET /:resource */
bb_error_t api_list(bb_request_t *req, bb_response_t *res)
{
    api_resource_t *r = find_resource(bb_request_get_param(req, "resource"));
    if (!r)
        return reply_error(res, 404, "unknown resource");

    bb_query_t q;
    int numbers[BB_QUERY_MAX_CONDITIONS];
    char err[128];

    if (build_query(r, req, &q, numbers, err, sizeof(err)) != 0)
        return reply_error(res, 400, "%s", err);

    void *rows = NULL;
    size_t count = 0;

    if (bb_repo_find(&r->repo, &q, &rows, &count) != 0)
        return reply_error(res, 500, "query failed");

    bb_json_t *array = bb_json_new_array();
    for (size_t i = 0; i < count; i++)
        bb_json_array_push(array,
            bb_entity_to_json(r->schema, (char *)rows + i * r->schema->struct_size));

    free(rows);
    return reply_json(res, 200, array);
}

/* POST /:resource */
bb_error_t api_create(bb_request_t *req, bb_response_t *res)
{
    api_resource_t *r = find_resource(bb_request_get_param(req, "resource"));
    if (!r)
        return reply_error(res, 404, "unknown resource");

    bb_schema_t *schema = r->schema;

    bb_json_t *json = parse_body(req);
    if (!json)
        return reply_error(res, 400, "request body must be a JSON object");

    char err[128];
    if (validate_body(schema, json, 1, err, sizeof(err)) != 0)
    {
        bb_json_destroy(json);
        return reply_error(res, 400, "%s", err);
    }

    void *entity = calloc(1, schema->struct_size);
    if (!entity)
    {
        bb_json_destroy(json);
        return reply_error(res, 500, "out of memory");
    }

    int rc = bb_json_to_entity(schema, json, entity);
    bb_json_destroy(json);

    if (rc != 0)
    {
        free(entity);
        return reply_error(res, 400, "invalid entity");
    }

    /* AUTO_GENERATE: nothing in the backends interprets this flag, so it is
     * applied here, from the schema. */
    for (size_t i = 0; i < schema->field_count; i++)
        if (is_server_owned(schema, i, 1))
            bb_uuid_v4_string((char *)entity + schema->fields[i].offset);

    if (bb_repo_insert(&r->repo, entity) != 0)
    {
        free(entity);
        return reply_error(res, 500, "could not store the entity");
    }

    bb_json_t *out = bb_entity_to_json(schema, entity);
    free(entity);
    return reply_json(res, 201, out);
}

/* GET /:resource/:id */
bb_error_t api_get(bb_request_t *req, bb_response_t *res)
{
    api_resource_t *r = find_resource(bb_request_get_param(req, "resource"));
    if (!r)
        return reply_error(res, 404, "unknown resource");

    int number = 0;
    const void *key = NULL;
    if (parse_key(pk_field(r->schema), bb_request_get_param(req, "id"), &number, &key) != 0)
        return reply_error(res, 404, "not found");

    void *entity = calloc(1, r->schema->struct_size);
    if (!entity)
        return reply_error(res, 500, "out of memory");

    if (bb_repo_find_by_pk(&r->repo, entity, key) != 0)
    {
        free(entity);
        return reply_error(res, 404, "not found");
    }

    bb_json_t *out = bb_entity_to_json(r->schema, entity);
    free(entity);
    return reply_json(res, 200, out);
}

/* PUT /:resource/:id -- replaces the whole entity; optional fields left out
 * of the body are reset. The key always comes from the URL. */
bb_error_t api_replace(bb_request_t *req, bb_response_t *res)
{
    api_resource_t *r = find_resource(bb_request_get_param(req, "resource"));
    if (!r)
        return reply_error(res, 404, "unknown resource");

    bb_schema_t *schema = r->schema;
    bb_field_t *pk = pk_field(schema);

    int number = 0;
    const void *key = NULL;
    if (parse_key(pk, bb_request_get_param(req, "id"), &number, &key) != 0)
        return reply_error(res, 404, "not found");

    void *entity = calloc(1, schema->struct_size);
    if (!entity)
        return reply_error(res, 500, "out of memory");

    if (bb_repo_find_by_pk(&r->repo, entity, key) != 0)
    {
        free(entity);
        return reply_error(res, 404, "not found");
    }

    bb_json_t *json = parse_body(req);
    if (!json)
    {
        free(entity);
        return reply_error(res, 400, "request body must be a JSON object");
    }

    char err[128];
    if (validate_body(schema, json, 0, err, sizeof(err)) != 0)
    {
        bb_json_destroy(json);
        free(entity);
        return reply_error(res, 400, "%s", err);
    }

    int rc = bb_json_to_entity(schema, json, entity); /* zeroes, then fills */
    bb_json_destroy(json);

    if (rc != 0)
    {
        free(entity);
        return reply_error(res, 400, "invalid entity");
    }

    set_key(pk, entity, key);

    if (bb_repo_update(&r->repo, entity) != 0)
    {
        free(entity);
        return reply_error(res, 500, "could not store the entity");
    }

    bb_json_t *out = bb_entity_to_json(schema, entity);
    free(entity);
    return reply_json(res, 200, out);
}

/* DELETE /:resource/:id */
bb_error_t api_delete(bb_request_t *req, bb_response_t *res)
{
    api_resource_t *r = find_resource(bb_request_get_param(req, "resource"));
    if (!r)
        return reply_error(res, 404, "unknown resource");

    int number = 0;
    const void *key = NULL;
    if (parse_key(pk_field(r->schema), bb_request_get_param(req, "id"), &number, &key) != 0)
        return reply_error(res, 404, "not found");

    if (bb_repo_remove(&r->repo, key) != 0)
        return reply_error(res, 404, "not found");

    bb_response_set_status(res, 204);
    return BB_SUCCESS();
}

/* ---------------------------------------------------------------------------
 * Routes
 * ------------------------------------------------------------------------- */

void api_add_routes(bb_server_t *server)
{
    /* Routes are matched in order, so the fixed one comes first. */
    bb_server_add_route(server, "GET",    "/",              api_describe);
    bb_server_add_route(server, "GET",    "/:resource",     api_list);
    bb_server_add_route(server, "POST",   "/:resource",     api_create);
    bb_server_add_route(server, "GET",    "/:resource/:id", api_get);
    bb_server_add_route(server, "PUT",    "/:resource/:id", api_replace);
    bb_server_add_route(server, "DELETE", "/:resource/:id", api_delete);
}
