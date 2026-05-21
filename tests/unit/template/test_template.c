#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <blue-bird/template/template.h>
#include <blue-bird/utils/json.h>


static void test_plain_text_render(void)
{
    printf("\tTesting plain text rendering...\n");

    bb_error_t err;

    bb_template_t *tpl =
        bb_template_parse(
            "Hello World",
            &err
        );

    assert(tpl != NULL);

    bb_json_t *ctx = bb_json_new(BB_JSON_OBJECT);

    char *result =
        bb_template_render(
            tpl,
            ctx,
            &err
        );

    assert(result != NULL);
    assert(strcmp(result, "Hello World") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}


static void test_simple_variable_render(void)
{
    printf("\tTesting simple variable rendering...\n");

    bb_error_t err;

    bb_template_t *tpl =
        bb_template_parse(
            "Hello {{name}}",
            &err
        );

    assert(tpl != NULL);

    bb_json_t *ctx = OBJ(
        KEY("name", TEXT("Blue"))
    );

    char *result =
        bb_template_render(
            tpl,
            ctx,
            &err
        );

    assert(result != NULL);
    assert(strcmp(result, "Hello Blue") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}


static void test_nested_variable_render(void)
{
    printf("\tTesting nested variable rendering...\n");

    bb_error_t err;

    bb_template_t *tpl =
        bb_template_parse(
            "User: {{user.name}}",
            &err
        );

    assert(tpl != NULL);

    bb_json_t *user = OBJ(
        KEY("name", TEXT("BlueBird"))
    );

    bb_json_t *ctx = OBJ(
        KEY("user", user)
    );

    char *result =
        bb_template_render(
            tpl,
            ctx,
            &err
        );

    assert(result != NULL);
    assert(strcmp(result, "User: BlueBird") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}


static void test_missing_variable_render(void)
{
    printf("\tTesting missing variable rendering...\n");

    bb_error_t err;

    bb_template_t *tpl =
        bb_template_parse(
            "Hello {{missing}}",
            &err
        );

    assert(tpl != NULL);

    bb_json_t *ctx = bb_json_new(BB_JSON_OBJECT);

    char *result =
        bb_template_render(
            tpl,
            ctx,
            &err
        );

    assert(result != NULL);

    /*
     * Missing variables currently render
     * as empty strings.
     */
    assert(strcmp(result, "Hello ") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}


static void test_numeric_render(void)
{
    printf("\tTesting numeric rendering...\n");

    bb_error_t err;

    bb_template_t *tpl =
        bb_template_parse(
            "Port={{port}}",
            &err
        );

    assert(tpl != NULL);

    bb_json_t *ctx = OBJ(
        KEY("port", INT(8080))
    );

    char *result =
        bb_template_render(
            tpl,
            ctx,
            &err
        );

    assert(result != NULL);

    /*
     * Depending on JSON stringify behavior,
     * adjust if needed.
     */
    assert(strcmp(result, "Port=8080") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}


static void test_boolean_render(void)
{
    printf("\tTesting boolean rendering...\n");

    bb_error_t err;

    bb_template_t *tpl =
        bb_template_parse(
            "Enabled={{enabled}}",
            &err
        );

    assert(tpl != NULL);

    bb_json_t *ctx = OBJ(
        KEY("enabled", BOOL(1))
    );

    char *result =
        bb_template_render(
            tpl,
            ctx,
            &err
        );

    assert(result != NULL);

    assert(strcmp(result, "Enabled=true") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
}


static void test_invalid_template_parse(void)
{
    printf("\tTesting invalid template parsing...\n");

    bb_error_t err;

    bb_template_t *tpl =
        bb_template_parse(
            "Hello {{name",
            &err
        );

    assert(tpl == NULL);
}


static void test_escaped_delimiter(void)
{
    printf("\tTesting escaped delimiters...\n");

    bb_error_t err;

    bb_template_t *tpl =
        bb_template_parse(
            "\\{{literal}}",
            &err
        );

    assert(tpl != NULL);

    bb_json_t *ctx = bb_json_new(BB_JSON_OBJECT);

    char *result =
        bb_template_render(
            tpl,
            ctx,
            &err
        );

    assert(result != NULL);

    /*
     * Current behavior:
     * escaped variable renders literally.
     */
    assert(strcmp(result, "{{literal}}") == 0);

    free(result);

    bb_json_destroy(ctx);
    bb_template_destroy(tpl);
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
    printf("All template tests passed.\n");
    return 0;
}
