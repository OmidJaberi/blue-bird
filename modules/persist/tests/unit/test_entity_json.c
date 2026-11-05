#include "blue-bird/persist/serialization/entity_json.h"
#include <blue-bird/error/assert.h>
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

    BB_ASSERT(obj);

    BB_ASSERT(
        bb_json_get_value_integer(
            bb_json_object_get_value(obj, "id")
        ) == 1
    );

    BB_ASSERT(
        strcmp(
            bb_json_get_value_text(
                bb_json_object_get_value(obj, "name")
            ),
            "Alice"
        ) == 0
    );

    bb_json_destroy(obj);
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

    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);

    bb_json_object_set_value(obj, "id", bb_json_new_int(42));
    bb_json_object_set_value(obj,"name", bb_json_new_text("Bob"));

    User u;
    memset(&u, 0, sizeof(u));

    BB_ASSERT(
        bb_json_to_entity(
            &schema,
            obj,
            &u
        ) == 0
    );

    BB_ASSERT(u.id == 42);
    BB_ASSERT(strcmp(u.name, "Bob") == 0);

    bb_json_destroy(obj);
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

    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);

    /* WRONG TYPE: TEXT instead of INT */
    bb_json_object_set_value(obj, "id", bb_json_new_text("oops"));

    User u;

    BB_ASSERT(
        bb_json_to_entity(
            &schema,
            obj,
            &u
        ) != 0
    );

    bb_json_destroy(obj);
}
/* A string value as long as (or longer than) its field must not leave the
 * field unterminated, or any later strlen()/printf() runs off the end of it. */
static void test_json_to_entity_string_limits(void)
{
    printf("\tTesting JSON to Entity string limits...\n");
    typedef struct {
        char name[8];
        char tag[8];
    } Rec;

    bb_field_t fields[] = {
        { .name = "name", .type = BB_FIELD_STRING, .offset = offsetof(Rec, name),
          .size = sizeof(((Rec *)0)->name), .flags = BB_FIELD_NONE },
        { .name = "tag", .type = BB_FIELD_UUID, .offset = offsetof(Rec, tag),
          .size = sizeof(((Rec *)0)->tag), .flags = BB_FIELD_NONE }
    };

    bb_schema_t schema = {
        .name = "recs",
        .fields = fields,
        .field_count = 2,
        .struct_size = sizeof(Rec),
        .primary_key_index = 0
    };

    static const struct { const char *input; const char *expected; } cases[] = {
        { "short",                    "short"   },   /* fits easily             */
        { "1234567",                  "1234567" },   /* exactly size - 1: fits  */
        { "12345678",                 "1234567" },   /* exactly size: truncated */
        { "1234567890abcdefghij",     "1234567" },   /* much longer             */
        { "",                         ""        },
        { "aaaaa\xc3\xa9",            "aaaaa\xc3\xa9" }, /* 7 bytes: fits whole  */
        { "aaaaaa\xc3\xa9",           "aaaaaa"  },   /* cut would split the e-acute  */
        { "aaaa\xe2\x82\xac\xe2\x82\xac", "aaaa\xe2\x82\xac" } /* 3-byte chars, cut lands inside the second */
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        for (int field = 0; field < 2; field++)
        {
            bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);
            bb_json_object_set_value(obj, fields[field].name, bb_json_new_text(cases[i].input));

            Rec r;
            memset(&r, 0x55, sizeof(r)); /* garbage: anything not set must be cleared */

            BB_ASSERT(bb_json_to_entity(&schema, obj, &r) == 0);

            const char *got = field == 0 ? r.name : r.tag;
            size_t size = sizeof(r.name);

            /* terminated inside the field, with exactly the expected content */
            BB_ASSERT(memchr(got, '\0', size) != NULL);
            BB_ASSERT(strcmp(got, cases[i].expected) == 0);

            /* the other field was not set by this JSON, so it is empty */
            const char *other = field == 0 ? r.tag : r.name;
            BB_ASSERT(other[0] == '\0');

            bb_json_destroy(obj);
        }
    }

    /* and what comes back out is the truncated value, not a read past the field */
    bb_json_t *obj = bb_json_create(BB_JSON_OBJECT);
    bb_json_object_set_value(obj, "name", bb_json_new_text("1234567890"));

    Rec r;
    BB_ASSERT(bb_json_to_entity(&schema, obj, &r) == 0);
    bb_json_destroy(obj);

    bb_json_t *back = bb_entity_to_json(&schema, &r);
    BB_ASSERT(back);
    BB_ASSERT(strcmp(bb_json_get_value_text(bb_json_object_get_value(back, "name")), "1234567") == 0);
    bb_json_destroy(back);
}

int main(void)
{
    printf("Running Entity_JSON tests...\n");

    test_entity_to_json();
    test_json_to_entity();
    test_json_to_entity_invalid_type();
    test_json_to_entity_string_limits();

    printf("All tests passed.\n");
    return 0;
}
