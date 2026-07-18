#include <blue-bird/error/assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <blue-bird/template/template.h>
#include <blue-bird/utils/json.h>


static void test_plain_text_render(void)
{
    printf("\tTesting plain text rendering...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "Hello World",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *ctx = bb_json_new_object();

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);
    BB_ASSERT(strcmp(result, "Hello World") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_simple_variable_render(void)
{
    printf("\tTesting simple variable rendering...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "Hello {{name}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *ctx = OBJ(
        KEY("name", TEXT("Blue"))
    );

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);
    BB_ASSERT(strcmp(result, "Hello Blue") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_nested_variable_render(void)
{
    printf("\tTesting nested variable rendering...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "User: {{user.name}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *user = OBJ(
        KEY("name", TEXT("BlueBird"))
    );

    bb_json_t *ctx = OBJ(
        KEY("user", user)
    );

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);
    BB_ASSERT(strcmp(result, "User: BlueBird") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_missing_variable_render(void)
{
    printf("\tTesting missing variable rendering...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "Hello {{missing}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *ctx = bb_json_new_object();

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    /*
     * Missing variables currently render
     * as empty strings.
     */
    BB_ASSERT(strcmp(result, "Hello ") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_numeric_render(void)
{
    printf("\tTesting numeric rendering...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "Port={{port}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *ctx = OBJ(
        KEY("port", INT(8080))
    );

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    /*
     * Depending on JSON stringify behavior,
     * adjust if needed.
     */
    BB_ASSERT(strcmp(result, "Port=8080") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_boolean_render(void)
{
    printf("\tTesting boolean rendering...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "Enabled={{enabled}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *ctx = OBJ(
        KEY("enabled", BOOL(1))
    );

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    BB_ASSERT(strcmp(result, "Enabled=true") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_invalid_template_parse(void)
{
    printf("\tTesting invalid template parsing...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "Hello {{name",
        &tpl
    );

    BB_ASSERT(tpl == NULL);
}

static void test_escaped_delimiter(void)
{
    printf("\tTesting escaped delimiters...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "\\{{literal}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *ctx = bb_json_new_object();

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    /*
     * Current behavior:
     * escaped variable renders literally.
     */
    BB_ASSERT(strcmp(result, "{{literal}}") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_section_render(void)
{
    printf("\tTesting section rendering...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "{{#items}}- {{name}}\n{{/items}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *items = bb_json_new_array();

    bb_json_array_push(
        items,
        OBJ(
            KEY("name", TEXT("Alpha"))
        )
    );

    bb_json_array_push(
        items,
        OBJ(
            KEY("name", TEXT("Beta"))
        )
    );

    bb_json_t *ctx = OBJ(
        KEY("items", items)
    );

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    BB_ASSERT(strcmp(result, "- Alpha\n- Beta\n") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_nested_sections(void)
{
    printf("\tTesting nested sections...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "{{#users}}"
        "User: {{name}}\n"
        "{{#posts}}"
        "* {{title}}\n"
        "{{/posts}}"
        "{{/users}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *posts = bb_json_new_array();

    bb_json_array_push(
        posts,
        OBJ(
            KEY("title", TEXT("Post A"))
        )
    );

    bb_json_array_push(
        posts,
        OBJ(
            KEY("title", TEXT("Post B"))
        )
    );

    bb_json_t *users = bb_json_new_array();

    bb_json_array_push(
        users,
        OBJ(
            KEY("name", TEXT("Blue")),
            KEY("posts", posts)
        )
    );

    bb_json_t *ctx = OBJ(
        KEY("users", users)
    );

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    BB_ASSERT(
        strcmp(
            result,
            "User: Blue\n"
            "* Post A\n"
            "* Post B\n"
        ) == 0
    );

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_conditional_truthy(void)
{
    printf("\tTesting truthy conditional...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "{{?logged_in}}Welcome{{/logged_in}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *ctx = OBJ(
        KEY("logged_in", BOOL(1))
    );

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    BB_ASSERT(strcmp(result, "Welcome") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_conditional_falsy(void)
{
    printf("\tTesting falsy conditional...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "{{?logged_in}}Welcome{{/logged_in}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *ctx = OBJ(
        KEY("logged_in", BOOL(0))
    );

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    BB_ASSERT(strcmp(result, "") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_comment_ignored(void)
{
    printf("\tTesting comment ignoring...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "Hello {{! comment }}World",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *ctx = bb_json_new_object();

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    BB_ASSERT(strcmp(result, "Hello World") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_parent_context_lookup(void)
{
    printf("\tTesting parent context lookup...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "{{#users}}"
        "{{name}} @ {{site}}\n"
        "{{/users}}",
        &tpl
    );

    BB_ASSERT(tpl != NULL);

    bb_json_t *users = bb_json_new_array();

    bb_json_array_push(
        users,
        OBJ(
            KEY("name", TEXT("Blue"))
        )
    );

    bb_json_t *ctx = OBJ(
        KEY("site", TEXT("BlueBird")),
        KEY("users", users)
    );

    char *result;
    bb_template_render(tpl, ctx, &result);

    BB_ASSERT(result != NULL);

    BB_ASSERT(
        strcmp(
            result,
            "Blue @ BlueBird\n"
        ) == 0
    );

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}

static void test_mismatched_closing_tag(void)
{
    printf("\tTesting mismatched closing tags...\n");

    bb_template_t *tpl;

    bb_template_parse(
        "{{#users}}{{/posts}}",
        &tpl
    );

    BB_ASSERT(tpl == NULL);
}

int main(void)
{
    printf("Running Template tests...\n");
    test_plain_text_render();
    test_simple_variable_render();
    test_nested_variable_render();
    test_missing_variable_render();
    test_numeric_render();
    test_boolean_render();
    test_invalid_template_parse();
    test_escaped_delimiter();
    test_section_render();
    test_nested_sections();
    test_conditional_truthy();
    test_conditional_falsy();
    test_comment_ignored();
    test_parent_context_lookup();
    test_mismatched_closing_tag();
    printf("All template tests passed.\n");
    return 0;
}
