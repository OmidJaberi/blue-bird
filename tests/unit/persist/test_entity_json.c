#include "blue-bird/persist/serialization/entity_json.h"
#include <assert.h>
#include <stdio.h>

static void test_entity_to_json(void)
{
    printf("\tTesting Entity to JSON...\n");
    typedef struct {
        int id;
        char name[64];
    } User;

    bb_field_t fields[] = {
        {
            .name = "id",
            .type = BB_FIELD_INT,
            .offset = offsetof(User, id),
            .size = sizeof(int),
            .flags = BB_FIELD_NONE
        },
        {
            .name = "name",
            .type = BB_FIELD_STRING,
            .offset = offsetof(User, name),
            .size = 64,
            .flags = BB_FIELD_NONE
        }
    };

    bb_schema_t schema = {
        .name = "users",
        .fields = fields,
        .field_count = 2,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    User u = {
        .id = 1
    };

    strcpy(u.name, "Alice");

    bb_json_t *obj = bb_entity_to_json(&schema, &u);

    assert(obj);

    assert(
        bb_json_get_value_integer(
            bb_json_object_get_value(obj, "id")
        ) == 1
    );

    assert(
        strcmp(
            bb_json_get_value_text(
                bb_json_object_get_value(obj, "name")
            ),
            "Alice"
        ) == 0
    );

    bb_json_destroy(obj);
    free(obj);
}

static void test_json_to_entity(void)
{
    printf("\tTesting JSON to Entity...\n");
    typedef struct {
        int id;
        char name[64];
    } User;

    bb_field_t fields[] = {
        {
            .name = "id",
            .type = BB_FIELD_INT,
            .offset = offsetof(User, id),
            .size = sizeof(int),
            .flags = BB_FIELD_NONE
        },
        {
            .name = "name",
            .type = BB_FIELD_STRING,
            .offset = offsetof(User, name),
            .size = 64,
            .flags = BB_FIELD_NONE
        }
    };

    bb_schema_t schema = {
        .name = "users",
        .fields = fields,
        .field_count = 2,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    bb_json_t *obj = bb_json_new(BB_JSON_OBJECT);

    bb_json_object_set_value(obj, "id", bb_json_new_int(42));
    bb_json_object_set_value(obj,"name", bb_json_new_text("Bob"));

    User u;
    memset(&u, 0, sizeof(u));

    assert(
        bb_json_to_entity(
            &schema,
            obj,
            &u
        ) == 0
    );

    assert(u.id == 42);
    assert(strcmp(u.name, "Bob") == 0);

    bb_json_destroy(obj);
    free(obj);
}

static void test_json_to_entity_invalid_type(void)
{
    printf("\tTesting JSON to Entity with invalid type...\n");
    typedef struct {
        int id;
    } User;

    bb_field_t fields[] = {
        {
            .name = "id",
            .type = BB_FIELD_INT,
            .offset = offsetof(User, id),
            .size = sizeof(int),
            .flags = BB_FIELD_NONE
        }
    };

    bb_schema_t schema = {
        .name = "users",
        .fields = fields,
        .field_count = 1,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    bb_json_t *obj = bb_json_new(BB_JSON_OBJECT);

    /* WRONG TYPE: TEXT instead of INT */
    bb_json_object_set_value(obj, "id", bb_json_new_text("oops"));

    User u;

    assert(
        bb_json_to_entity(
            &schema,
            obj,
            &u
        ) != 0
    );

    bb_json_destroy(obj);
    free(obj);
}
int main(void)
{
    printf("Running Entity_JSON tests...\n");

    test_entity_to_json();
    test_json_to_entity();
    test_json_to_entity_invalid_type();

    printf("All tests passed.\n");
    return 0;
}
