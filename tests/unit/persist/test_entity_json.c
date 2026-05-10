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

    BB_Field fields[] = {
        {
            "id",
            BB_FIELD_INT,
            offsetof(User, id),
            sizeof(int)
        },
        {
            "name",
            BB_FIELD_STRING,
            offsetof(User, name),
            64
        }
    };

    BB_Schema schema = {
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

    json_node_t *obj = bb_entity_to_json(&schema, &u);

    assert(obj);

    assert(
        get_json_integer_value(
            get_json_object_value(obj, "id")
        ) == 1
    );

    assert(
        strcmp(
            get_json_text_value(
                get_json_object_value(obj, "name")
            ),
            "Alice"
        ) == 0
    );

    destroy_json(obj);
    free(obj);
}

static void test_json_to_entity(void)
{
    printf("\tTesting JSON to Entity...\n");
    typedef struct {
        int id;
        char name[64];
    } User;

    BB_Field fields[] = {
        {
            "id",
            BB_FIELD_INT,
            offsetof(User, id),
            sizeof(int)
        },
        {
            "name",
            BB_FIELD_STRING,
            offsetof(User, name),
            64
        }
    };

    BB_Schema schema = {
        .name = "users",
        .fields = fields,
        .field_count = 2,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    json_node_t *obj = json_new(JSON_OBJECT);

    set_json_object_value(obj, "id", json_new_int(42));
    set_json_object_value(obj,"name", json_new_text("Bob"));

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

    destroy_json(obj);
    free(obj);
}

static void test_json_to_entity_invalid_type(void)
{
    printf("\tTesting JSON to Entity with invalid type...\n");
    typedef struct {
        int id;
    } User;

    BB_Field fields[] = {
        {
            "id",
            BB_FIELD_INT,
            offsetof(User, id),
            sizeof(int)
        }
    };

    BB_Schema schema = {
        .name = "users",
        .fields = fields,
        .field_count = 1,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    json_node_t *obj = json_new(JSON_OBJECT);

    /* WRONG TYPE: TEXT instead of INT */
    set_json_object_value(obj, "id", json_new_text("oops"));

    User u;

    assert(
        bb_json_to_entity(
            &schema,
            obj,
            &u
        ) != 0
    );

    destroy_json(obj);
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
