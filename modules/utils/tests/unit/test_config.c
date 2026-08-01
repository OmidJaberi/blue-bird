#include "blue-bird/utils/bb_config.h"
#include <blue-bird/error/assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_test_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    BB_ASSERT(f != NULL);

    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

void test_config_load_env(void)
{
    printf("\tTesting config .env loading...\n");

    const char *path = "test_config.env";

    write_test_file(
        path,
        "HOST=localhost\n"
        "PORT=8080\n"
        "DEBUG=true\n"
    );

    bb_json_t *config = bb_json_create(BB_JSON_OBJECT);

    BB_ASSERT(!BB_FAILED(bb_config_load_env(config, path)));

    // HOST is text
    BB_ASSERT(strcmp(bb_json_get_value_text(bb_json_object_get_value(config, "HOST")), "localhost") == 0);

    // PORT is parsed as an integer
    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(config, "PORT")) == 8080);

    // DEBUG is parsed as a boolean
    BB_ASSERT(bb_json_get_value_bool(bb_json_object_get_value(config, "DEBUG")) == true);

    remove(path);
    bb_json_destroy(config);
}

void test_config_env_overwrite(void)
{
    printf("\tTesting config .env overwrite...\n");

    const char *path = "test_config_overwrite.env";

    write_test_file(path, "NAME=new_value\n");

    bb_json_t *config = bb_json_create(BB_JSON_OBJECT);

    bb_json_object_set_value(config, "NAME", bb_json_new_text("old_value"));

    BB_ASSERT(!BB_FAILED(bb_config_load_env(config, path)));

    BB_ASSERT(strcmp(bb_json_get_value_text(bb_json_object_get_value(config, "NAME")), "new_value") == 0);

    remove(path);
    bb_json_destroy(config);
}

void test_config_load_json(void)
{
    printf("\tTesting config JSON loading...\n");

    const char *path = "test_config.json";

    write_test_file(
        path,
        "{"
        "\"host\": \"127.0.0.1\","
        "\"port\": 3000,"
        "\"enabled\": true"
        "}"
    );

    bb_json_t *config = bb_json_create(BB_JSON_OBJECT);

    BB_ASSERT(!BB_FAILED(bb_config_load_json(config, path)));

    BB_ASSERT(strcmp(bb_json_get_value_text(bb_json_object_get_value(config, "host")), "127.0.0.1") == 0);

    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(config, "port")) == 3000);

    BB_ASSERT(bb_json_get_value_bool(bb_json_object_get_value(config, "enabled")) == true);

    remove(path);
    bb_json_destroy(config);
}

void test_config_json_overwrite(void)
{
    printf("\tTesting config JSON overwrite...\n");

    const char *path = "test_config_overwrite.json";

    write_test_file(
        path,
        "{"
        "\"value\": 42"
        "}"
    );

    bb_json_t *config = bb_json_create(BB_JSON_OBJECT);

    bb_json_object_set_value(config, "value", bb_json_new_int(10));

    BB_ASSERT(!BB_FAILED(bb_config_load_json(config, path)));

    BB_ASSERT(bb_json_get_value_integer(bb_json_object_get_value(config, "value")) == 42);

    remove(path);
    bb_json_destroy(config);
}

void test_config_invalid_destination(void)
{
    printf("\tTesting config invalid destination...\n");

    const char *path = "test_config_invalid.env";

    write_test_file(path, "VALUE=test\n");

    bb_json_t *array = bb_json_create(BB_JSON_ARRAY);

    BB_ASSERT(BB_FAILED(bb_config_load_env(array, path)));

    remove(path);
    bb_json_destroy(array);
}

int main(void)
{
    printf("Running config tests...\n");

    test_config_load_env();
    test_config_env_overwrite();

    test_config_load_json();
    test_config_json_overwrite();

    test_config_invalid_destination();

    printf("All tests passed.\n");
    return 0;
}
