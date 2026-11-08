#include "blue-bird/persist/schema.h"
#include <blue-bird/error/assert.h>
#include <blue-bird/utils/uuid.h>
#include <stdio.h>
#include <string.h>

static void test_valid_schema(void)
{
    printf("\tTesting valid schema...\n");
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

    BB_ASSERT(bb_schema_validate(&schema) == 0);
}

static void test_duplicate_field_names(void)
{
    printf("\tTesting duplicate field names...\n");
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
        },
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
        .field_count = 2,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    BB_ASSERT(bb_schema_validate(&schema) != 0);
}

static void test_relationship_validation(void)
{
    printf("\tTesting relationship validation...\n");
    typedef struct {
        char id[37];
    } User;

    typedef struct {
        char id[37];
        char user_id[37];
    } Task;

    bb_field_t user_fields[] = {
        {
            .name = "id",
            .type = BB_FIELD_UUID,
            .offset = offsetof(User, id),
            .size = 37,
            .flags = BB_FIELD_NONE
        }
    };

    bb_schema_t user_schema = {
        .name = "users",
        .fields = user_fields,
        .field_count = 1,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    BB_ASSERT(bb_schema_register(&user_schema) == 0);

    bb_field_t task_fields[] = {
        {
            .name = "id",
            .type = BB_FIELD_UUID,
            .offset = offsetof(Task, id),
            .size = 37,
            .flags = BB_FIELD_NONE
        },
        {
            .name = "user_id",
            .type = BB_FIELD_UUID,
            .offset = offsetof(Task, user_id),
            .size = 37,
            .flags = BB_FIELD_NONE,

            .references_schema = "users",
            .references_field = "id"
        }
    };

    bb_schema_t task_schema = {
        .name = "tasks",
        .fields = task_fields,
        .field_count = 2,
        .struct_size = sizeof(Task),
        .primary_key_index = 0
    };

    BB_ASSERT(bb_schema_validate(&task_schema) == 0);
}

/* ---------------------------
 * AUTO_GENERATE
 * --------------------------- */

typedef struct {
    bb_uuid_t id;
    bb_uuid_t other;
    char name[16];
} Gen;

static bb_field_t gen_fields[] = {
    { .name = "id",    .type = BB_FIELD_UUID,   .offset = offsetof(Gen, id),    .size = sizeof(bb_uuid_t),
      .flags = BB_FIELD_PRIMARY_KEY | BB_FIELD_AUTO_GENERATE },
    { .name = "other", .type = BB_FIELD_UUID,   .offset = offsetof(Gen, other), .size = sizeof(bb_uuid_t),
      .flags = BB_FIELD_AUTO_GENERATE },
    { .name = "name",  .type = BB_FIELD_STRING, .offset = offsetof(Gen, name),  .size = 16,
      .flags = BB_FIELD_NONE }
};

static bb_schema_t gen_schema = {
    .name = "gens",
    .fields = gen_fields,
    .field_count = 3,
    .struct_size = sizeof(Gen),
    .primary_key_index = 0
};

static void test_auto_generate_fills_empty_uuid_fields(void)
{
    printf("\tTesting AUTO_GENERATE fills empty UUID fields...\n");

    BB_ASSERT(bb_schema_validate(&gen_schema) == 0);

    Gen g;
    memset(&g, 0, sizeof(g));
    strcpy(g.name, "untouched");

    BB_ASSERT(bb_schema_apply_auto_generate(&gen_schema, &g) == 0);
    BB_ASSERT(strlen(g.id) == 36);
    BB_ASSERT(strlen(g.other) == 36);
    BB_ASSERT(strcmp(g.id, g.other) != 0);          /* independent values */
    BB_ASSERT(strcmp(g.name, "untouched") == 0);    /* unflagged fields are left alone */

    /* a second call changes nothing: the fields are no longer empty */
    Gen again = g;
    BB_ASSERT(bb_schema_apply_auto_generate(&gen_schema, &again) == 0);
    BB_ASSERT(memcmp(&again, &g, sizeof(g)) == 0);
}

static void test_auto_generate_keeps_preset_values(void)
{
    printf("\tTesting AUTO_GENERATE keeps a value the caller set...\n");

    Gen g;
    memset(&g, 0, sizeof(g));
    strcpy(g.id, "my-own-id");

    BB_ASSERT(bb_schema_apply_auto_generate(&gen_schema, &g) == 0);
    BB_ASSERT(strcmp(g.id, "my-own-id") == 0);
    BB_ASSERT(strlen(g.other) == 36);               /* the other, empty, field is filled */

    BB_ASSERT(bb_schema_apply_auto_generate(&gen_schema, NULL) != 0);
    BB_ASSERT(bb_schema_apply_auto_generate(NULL, &g) != 0);
}

static void test_auto_generate_only_valid_on_uuid_fields(void)
{
    printf("\tTesting AUTO_GENERATE is rejected on anything but a UUID field...\n");

    typedef struct {
        int id;
        bb_uuid_t small;
    } Bad;

    /* an int field */
    bb_field_t int_fields[] = {
        { .name = "id", .type = BB_FIELD_INT, .offset = offsetof(Bad, id), .size = sizeof(int),
          .flags = BB_FIELD_PRIMARY_KEY | BB_FIELD_AUTO_GENERATE }
    };
    bb_schema_t int_schema = {
        .name = "bad_int", .fields = int_fields, .field_count = 1,
        .struct_size = sizeof(Bad), .primary_key_index = 0
    };
    BB_ASSERT(bb_schema_validate(&int_schema) != 0);

    /* a UUID field too small to hold one */
    bb_field_t small_fields[] = {
        { .name = "id", .type = BB_FIELD_UUID, .offset = offsetof(Bad, small), .size = 8,
          .flags = BB_FIELD_PRIMARY_KEY | BB_FIELD_AUTO_GENERATE }
    };
    bb_schema_t small_schema = {
        .name = "bad_small", .fields = small_fields, .field_count = 1,
        .struct_size = sizeof(Bad), .primary_key_index = 0
    };
    BB_ASSERT(bb_schema_validate(&small_schema) != 0);

    /* and applying it anyway refuses to write out of bounds */
    Bad b;
    memset(&b, 0, sizeof(b));
    BB_ASSERT(bb_schema_apply_auto_generate(&small_schema, &b) != 0);
    BB_ASSERT(bb_schema_apply_auto_generate(&int_schema, &b) != 0);
}

int main(void)
{
    printf("Running schema tests...\n");

    test_valid_schema();
    test_duplicate_field_names();
    test_relationship_validation();
    test_auto_generate_fills_empty_uuid_fields();
    test_auto_generate_keeps_preset_values();
    test_auto_generate_only_valid_on_uuid_fields();

    printf("All tests passed.\n");
    return 0;
}
