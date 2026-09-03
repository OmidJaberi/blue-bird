#include "generators/persist_schema.h"
#include "build_config.h"

#include <blue-bird/template/template.h>
#include <blue-bird/utils/json.h>
#include <blue-bird/utils/platform.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------
 * Small helpers
 * --------------------------- */

static char *bb_codegen_read_file_or_null(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < 0)
    {
        fclose(f);
        return NULL;
    }

    char *buf = malloc((size_t)size + 1);
    if (!buf)
    {
        fclose(f);
        return NULL;
    }

    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';

    return buf;
}

static int bb_codegen_write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    if (!f)
    {
        fprintf(stderr, "bb-codegen: failed to open '%s' for writing\n", path);
        return -1;
    }

    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, f);
    fclose(f);

    if (written != len)
    {
        fprintf(stderr, "bb-codegen: failed to write '%s'\n", path);
        return -1;
    }

    return 0;
}

static int bb_codegen_is_valid_identifier(const char *s)
{
    if (!s || !*s)
        return 0;

    if (!(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return 0;

    for (const char *p = s + 1; *p; p++)
    {
        if (!(isalnum((unsigned char)*p) || *p == '_'))
            return 0;
    }

    return 1;
}

static char *bb_codegen_to_upper_copy(const char *s)
{
    size_t len = strlen(s);
    char *out = malloc(len + 1);
    for (size_t i = 0; i < len; i++)
        out[i] = (char)toupper((unsigned char)s[i]);
    out[len] = '\0';
    return out;
}

static int bb_codegen_bool_field(bb_json_t *obj, const char *key)
{
    bb_json_t *v = bb_json_object_get_value(obj, key);
    return v && bb_json_get_type(v) == BB_JSON_BOOL && bb_json_get_value_bool(v);
}

/* Generated output must be byte-identical regardless of how the
 * manifest path was spelled on argv (relative from one CWD, absolute
 * from another, ./foo vs foo, ...) or `generate` and `check` will
 * disagree about whether output is stale for reasons that have nothing
 * to do with the manifest actually changing. Only the basename is
 * invocation-invariant, so that's what goes in the generated comment. */
static const char *bb_codegen_basename(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last = slash > backslash ? slash : backslash;
    return last ? last + 1 : path;
}

/* ---------------------------
 * Manifest -> view model
 *
 * The view model is plain data for the templates to walk: every string
 * that needs to render as C source verbatim (a quoted literal, a flags
 * expression, a type declaration) is fully pre-computed here, since the
 * template engine deliberately has no filters/helpers/expressions
 * (see docs/template/overview.md) -- it only does variable lookup,
 * loops over arrays, and truthy conditionals.
 * --------------------------- */

static int bb_codegen_build_field_view(bb_json_t *field_json,
                                       size_t index,
                                       const char *manifest_path,
                                       bb_json_t *out_field,
                                       int *is_primary_key_out,
                                       int *is_uuid_out)
{
    bb_json_t *name_j = bb_json_object_get_value(field_json, "name");

    if (!name_j || bb_json_get_type(name_j) != BB_JSON_TEXT)
    {
        fprintf(stderr, "bb-codegen: %s: fields[%zu] is missing a string \"name\"\n",
                manifest_path, index);
        return -1;
    }

    const char *name = bb_json_get_value_text(name_j);

    if (!bb_codegen_is_valid_identifier(name))
    {
        fprintf(stderr, "bb-codegen: %s: field \"%s\" is not a valid C identifier\n",
                manifest_path, name);
        return -1;
    }

    bb_json_t *type_j = bb_json_object_get_value(field_json, "type");

    if (!type_j || bb_json_get_type(type_j) != BB_JSON_TEXT)
    {
        fprintf(stderr, "bb-codegen: %s: field \"%s\" is missing a string \"type\"\n",
                manifest_path, name);
        return -1;
    }

    const char *type = bb_json_get_value_text(type_j);

    char c_decl[256];
    char size_expr[64];
    const char *type_enum;
    int is_uuid = 0;

    if (strcmp(type, "int") == 0)
    {
        snprintf(c_decl, sizeof(c_decl), "int %s;", name);
        snprintf(size_expr, sizeof(size_expr), "sizeof(int)");
        type_enum = "BB_FIELD_INT";
    }
    else if (strcmp(type, "uuid") == 0)
    {
        snprintf(c_decl, sizeof(c_decl), "bb_uuid_t %s;", name);
        snprintf(size_expr, sizeof(size_expr), "BB_UUID_BUF_LEN");
        type_enum = "BB_FIELD_UUID";
        is_uuid = 1;
    }
    else if (strcmp(type, "string") == 0 || strcmp(type, "blob") == 0)
    {
        bb_json_t *size_j = bb_json_object_get_value(field_json, "size");

        if (!size_j || bb_json_get_type(size_j) != BB_JSON_INT || bb_json_get_value_integer(size_j) <= 0)
        {
            fprintf(stderr,
                    "bb-codegen: %s: field \"%s\" of type \"%s\" requires a positive integer \"size\"\n",
                    manifest_path, name, type);
            return -1;
        }

        int size = bb_json_get_value_integer(size_j);

        if (strcmp(type, "string") == 0)
        {
            snprintf(c_decl, sizeof(c_decl), "char %s[%d];", name, size);
            type_enum = "BB_FIELD_STRING";
        }
        else
        {
            snprintf(c_decl, sizeof(c_decl), "unsigned char %s[%d];", name, size);
            type_enum = "BB_FIELD_BLOB";
        }

        snprintf(size_expr, sizeof(size_expr), "%d", size);
    }
    else
    {
        fprintf(stderr,
                "bb-codegen: %s: field \"%s\" has unknown type \"%s\" (expected int/string/uuid/blob)\n",
                manifest_path, name, type);
        return -1;
    }

    int primary_key   = bb_codegen_bool_field(field_json, "primary_key");
    int required      = bb_codegen_bool_field(field_json, "required");
    int unique        = bb_codegen_bool_field(field_json, "unique");
    int auto_generate = bb_codegen_bool_field(field_json, "auto_generate");

    char flags_expr[160] = "";
    int have_flag = 0;

#define BB_APPEND_FLAG(cond, literal) \
    if (cond) { \
        if (have_flag) strcat(flags_expr, " | "); \
        strcat(flags_expr, literal); \
        have_flag = 1; \
    }

    BB_APPEND_FLAG(primary_key, "BB_FIELD_PRIMARY_KEY")
    BB_APPEND_FLAG(required, "BB_FIELD_REQUIRED")
    BB_APPEND_FLAG(unique, "BB_FIELD_UNIQUE")
    BB_APPEND_FLAG(auto_generate, "BB_FIELD_AUTO_GENERATE")

#undef BB_APPEND_FLAG

    if (!have_flag)
        strcpy(flags_expr, "BB_FIELD_NONE");

    char name_literal[128];
    snprintf(name_literal, sizeof(name_literal), "\"%s\"", name);

    char references_schema_expr[128] = "NULL";
    char references_field_expr[128] = "NULL";

    bb_json_t *references = bb_json_object_get_value(field_json, "references");
    if (references && bb_json_get_type(references) == BB_JSON_OBJECT)
    {
        bb_json_t *ref_schema = bb_json_object_get_value(references, "schema");
        bb_json_t *ref_field = bb_json_object_get_value(references, "field");

        if (!ref_schema || bb_json_get_type(ref_schema) != BB_JSON_TEXT ||
            !ref_field || bb_json_get_type(ref_field) != BB_JSON_TEXT)
        {
            fprintf(stderr,
                    "bb-codegen: %s: field \"%s\" has a \"references\" object but is missing "
                    "string \"schema\"/\"field\" in it\n",
                    manifest_path, name);
            return -1;
        }

        snprintf(references_schema_expr, sizeof(references_schema_expr),
                 "\"%s\"", bb_json_get_value_text(ref_schema));
        snprintf(references_field_expr, sizeof(references_field_expr),
                 "\"%s\"", bb_json_get_value_text(ref_field));
    }

    bb_json_object_set_value(out_field, "name", TEXTV(name));
    bb_json_object_set_value(out_field, "name_literal", TEXTV(name_literal));
    bb_json_object_set_value(out_field, "c_decl", TEXTV(c_decl));
    bb_json_object_set_value(out_field, "type_enum", TEXTV(type_enum));
    bb_json_object_set_value(out_field, "size_expr", TEXTV(size_expr));
    bb_json_object_set_value(out_field, "flags_expr", TEXTV(flags_expr));
    bb_json_object_set_value(out_field, "references_schema_expr", TEXTV(references_schema_expr));
    bb_json_object_set_value(out_field, "references_field_expr", TEXTV(references_field_expr));

    *is_primary_key_out = primary_key;
    *is_uuid_out = is_uuid;

    return 0;
}

static int bb_codegen_build_view_model(bb_json_t *manifest, const char *manifest_path, bb_json_t **out_model)
{
    const char *struct_name = bb_json_get_value_text(bb_json_object_get_value(manifest, "name"));

    if (!bb_codegen_is_valid_identifier(struct_name))
    {
        fprintf(stderr, "bb-codegen: %s: \"name\" (\"%s\") is not a valid C identifier\n",
                manifest_path, struct_name);
        return -1;
    }

    bb_json_t *table_j = bb_json_object_get_value(manifest, "table");
    const char *table_name = (table_j && bb_json_get_type(table_j) == BB_JSON_TEXT)
                                  ? bb_json_get_value_text(table_j)
                                  : struct_name;

    bb_json_t *fields_j = bb_json_object_get_value(manifest, "fields");
    if (!fields_j || bb_json_get_type(fields_j) != BB_JSON_ARRAY || bb_json_get_size(fields_j) == 0)
    {
        fprintf(stderr, "bb-codegen: %s: \"fields\" must be a non-empty array\n", manifest_path);
        return -1;
    }

    size_t field_count = bb_json_get_size(fields_j);

    bb_json_t *fields_view = bb_json_new_array();
    long primary_key_index = -1;
    int needs_uuid = 0;

    for (size_t i = 0; i < field_count; i++)
    {
        bb_json_t *field_json = bb_json_array_get_index(fields_j, (unsigned int)i);
        bb_json_t *field_view = bb_json_new_object();

        int is_primary_key = 0;
        int is_uuid = 0;

        if (bb_codegen_build_field_view(field_json, i, manifest_path, field_view,
                                        &is_primary_key, &is_uuid) != 0)
        {
            bb_json_destroy(field_view);
            bb_json_destroy(fields_view);
            return -1;
        }

        if (is_primary_key)
        {
            if (primary_key_index != -1)
            {
                fprintf(stderr,
                        "bb-codegen: %s: more than one field is marked \"primary_key\": true\n",
                        manifest_path);
                bb_json_destroy(field_view);
                bb_json_destroy(fields_view);
                return -1;
            }
            primary_key_index = (long)i;
        }

        if (is_uuid)
            needs_uuid = 1;

        bb_json_array_push(fields_view, field_view);
    }

    if (primary_key_index == -1)
    {
        fprintf(stderr,
                "bb-codegen: %s: exactly one field must be marked \"primary_key\": true (none found)\n",
                manifest_path);
        bb_json_destroy(fields_view);
        return -1;
    }

    /* Duplicate field names would silently break offsetof()/bb_schema_find_field(). */
    for (size_t i = 0; i < field_count; i++)
    {
        bb_json_t *fi = bb_json_array_get_index(fields_j, (unsigned int)i);
        const char *ni = bb_json_get_value_text(bb_json_object_get_value(fi, "name"));

        for (size_t j = i + 1; j < field_count; j++)
        {
            bb_json_t *fj = bb_json_array_get_index(fields_j, (unsigned int)j);
            const char *nj = bb_json_get_value_text(bb_json_object_get_value(fj, "name"));

            if (ni && nj && strcmp(ni, nj) == 0)
            {
                fprintf(stderr, "bb-codegen: %s: duplicate field name \"%s\"\n", manifest_path, ni);
                bb_json_destroy(fields_view);
                return -1;
            }
        }
    }

    char *header_guard_upper = bb_codegen_to_upper_copy(struct_name);
    char header_guard[192];
    snprintf(header_guard, sizeof(header_guard), "BB_GENERATED_%s_SCHEMA_H", header_guard_upper);
    free(header_guard_upper);

    char table_name_literal[128];
    snprintf(table_name_literal, sizeof(table_name_literal), "\"%s\"", table_name);

    bb_json_t *model = bb_json_new_object();
    bb_json_object_set_value(model, "manifest_path", TEXTV(bb_codegen_basename(manifest_path)));
    bb_json_object_set_value(model, "struct_name", TEXTV(struct_name));
    bb_json_object_set_value(model, "table_name_literal", TEXTV(table_name_literal));
    bb_json_object_set_value(model, "header_guard", TEXTV(header_guard));
    bb_json_object_set_value(model, "field_count", INTV((int)field_count));
    bb_json_object_set_value(model, "primary_key_index", INTV((int)primary_key_index));
    bb_json_object_set_value(model, "needs_uuid", BOOLV(needs_uuid));
    bb_json_object_set_value(model, "fields", fields_view);

    *out_model = model;
    return 0;
}

/* ---------------------------
 * Rendering
 * --------------------------- */

static int bb_codegen_render_template(const char *template_filename, bb_json_t *view_model, char **out)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", BB_CODEGEN_TEMPLATES_DIR, template_filename);

    bb_template_t *tpl = NULL;
    if (BB_FAILED(bb_template_parse_file(path, &tpl)))
    {
        fprintf(stderr, "bb-codegen: failed to load template '%s'\n", path);
        return -1;
    }

    bb_error_t err = bb_template_render(tpl, view_model, out);
    bb_template_destroy(tpl);

    if (BB_FAILED(err))
    {
        fprintf(stderr, "bb-codegen: failed to render template '%s'\n", path);
        return -1;
    }

    return 0;
}

static int bb_codegen_persist_schema_render(bb_json_t *manifest, const char *manifest_path,
                                            char **out_header, char **out_source,
                                            char *out_struct_name, size_t struct_name_buf_size)
{
    bb_json_t *view_model = NULL;
    if (bb_codegen_build_view_model(manifest, manifest_path, &view_model) != 0)
        return -1;

    const char *struct_name = bb_json_get_value_text(bb_json_object_get_value(view_model, "struct_name"));
    snprintf(out_struct_name, struct_name_buf_size, "%s", struct_name);

    int rc = bb_codegen_render_template("persist_schema.h.tmpl", view_model, out_header);

    if (rc == 0)
        rc = bb_codegen_render_template("persist_schema.c.tmpl", view_model, out_source);

    bb_json_destroy(view_model);
    return rc;
}

/* ---------------------------
 * generate() / check()
 * --------------------------- */

static int bb_codegen_persist_schema_generate(bb_json_t *manifest, const char *manifest_path, const char *out_dir)
{
    char *header = NULL;
    char *source = NULL;
    char struct_name[128];

    if (bb_codegen_persist_schema_render(manifest, manifest_path, &header, &source,
                                         struct_name, sizeof(struct_name)) != 0)
        return 1;

    bb_mkdir(out_dir);

    char header_path[1024];
    char source_path[1024];
    snprintf(header_path, sizeof(header_path), "%s/%s_schema.generated.h", out_dir, struct_name);
    snprintf(source_path, sizeof(source_path), "%s/%s_schema.generated.c", out_dir, struct_name);

    int rc = bb_codegen_write_file(header_path, header);
    if (rc == 0)
        rc = bb_codegen_write_file(source_path, source);

    if (rc == 0)
        printf("bb-codegen: wrote %s, %s\n", header_path, source_path);

    free(header);
    free(source);
    return rc;
}

static int bb_codegen_persist_schema_check(bb_json_t *manifest, const char *manifest_path, const char *out_dir)
{
    char *header = NULL;
    char *source = NULL;
    char struct_name[128];

    if (bb_codegen_persist_schema_render(manifest, manifest_path, &header, &source,
                                         struct_name, sizeof(struct_name)) != 0)
        return 1;

    char header_path[1024];
    char source_path[1024];
    snprintf(header_path, sizeof(header_path), "%s/%s_schema.generated.h", out_dir, struct_name);
    snprintf(source_path, sizeof(source_path), "%s/%s_schema.generated.c", out_dir, struct_name);

    int stale = 0;

    char *existing_header = bb_codegen_read_file_or_null(header_path);
    if (!existing_header || strcmp(existing_header, header) != 0)
    {
        fprintf(stderr, "bb-codegen: STALE (or missing): %s\n", header_path);
        stale = 1;
    }
    free(existing_header);

    char *existing_source = bb_codegen_read_file_or_null(source_path);
    if (!existing_source || strcmp(existing_source, source) != 0)
    {
        fprintf(stderr, "bb-codegen: STALE (or missing): %s\n", source_path);
        stale = 1;
    }
    free(existing_source);

    if (!stale)
        printf("bb-codegen: up to date: %s, %s\n", header_path, source_path);

    free(header);
    free(source);
    return stale;
}

const bb_codegen_generator_t bb_codegen_persist_schema_generator = {
    .generate = bb_codegen_persist_schema_generate,
    .check = bb_codegen_persist_schema_check
};
