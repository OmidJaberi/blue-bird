#include "blue-bird/persist/schema.h"
#include <assert.h>
#include <stdio.h>

static void test_valid_schema(void)
{
    printf("\tTesting valid schema...\n");
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

    assert(bb_schema_validate(&schema) == 0);
}

static void test_duplicate_field_names(void)
{
    printf("\tTesting duplicate field names...\n");
    typedef struct {
        int id;
    } User;

    BB_Field fields[] = {
        {
            "id",
            BB_FIELD_INT,
            offsetof(User, id),
            sizeof(int)
        },
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
        .field_count = 2,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    assert(bb_schema_validate(&schema) != 0);
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

    BB_Field user_fields[] = {
        {
            "id",
            BB_FIELD_UUID,
            offsetof(User, id),
            37
        }
    };

    BB_Schema user_schema = {
        .name = "users",
        .fields = user_fields,
        .field_count = 1,
        .struct_size = sizeof(User),
        .primary_key_index = 0
    };

    assert(bb_schema_register(&user_schema) == 0);

    BB_Field task_fields[] = {
        {
            "id",
            BB_FIELD_UUID,
            offsetof(Task, id),
            37
        },
        {
            "user_id",
            BB_FIELD_UUID,
            offsetof(Task, user_id),
            37,

            .references_schema = "users",
            .references_field = "id"
        }
    };

    BB_Schema task_schema = {
        .name = "tasks",
        .fields = task_fields,
        .field_count = 2,
        .struct_size = sizeof(Task),
        .primary_key_index = 0
    };

    assert(bb_schema_validate(&task_schema) == 0);
}

int main(void)
{
    printf("Running schema tests...\n");

    test_valid_schema();
    test_duplicate_field_names();
    test_relationship_validation();

    printf("All tests passed.\n");
    return 0;
}
