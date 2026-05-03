#include "persist/schema.h"
#include <assert.h>
#include <stdio.h>

static void test_valid_schema()
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

static void test_duplicate_field_names()
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

int main()
{
    printf("Running schema tests...\n");

    test_valid_schema();
    test_duplicate_field_names();

    printf("All tests passed.\n");
    return 0;
}