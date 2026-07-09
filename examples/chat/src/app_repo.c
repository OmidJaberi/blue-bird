#include "app_repo.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================
 * User schema
 * ============================================================ */

UserRepo global_user_repo;

bb_field_t user_fields[] = {
    {
        .name = "username",
        .type = BB_FIELD_STRING,
        .offset = offsetof(User, username),
        .size = APP_USERNAME_LEN,
        .flags = BB_FIELD_NONE
    },
    {
        .name = "password_hash",
        .type = BB_FIELD_STRING,
        .offset = offsetof(User, password_hash),
        .size = BB_PASSWORD_HASH_MAX,
        .flags = BB_FIELD_NONE
    }
};

bb_schema_t user_schema = {
    .name = "users",
    .fields = user_fields,
    .field_count = 2,
    .struct_size = sizeof(User),
    .primary_key_index = 0
};

int user_insert(UserRepo *repo, User *user)
{
    return bb_repo_insert(&repo->base, user);
}

int user_find_by_username(UserRepo *repo, User *out, const char *username)
{
    return bb_repo_find_by_pk(&repo->base, out, username);
}

int user_exists(UserRepo *repo, const char *username)
{
    User tmp = {0};
    return user_find_by_username(repo, &tmp, username) == 0;
}

/* ============================================================
 * Message schema
 * ============================================================ */

MessageRepo global_message_repo;

bb_field_t message_fields[] = {
    {
        .name = "id",
        .type = BB_FIELD_UUID,
        .offset = offsetof(Message, id),
        .size = BB_UUID_BUF_LEN,
        .flags = BB_FIELD_NONE
    },
    {
        .name = "from_user",
        .type = BB_FIELD_STRING,
        .offset = offsetof(Message, from_user),
        .size = APP_USERNAME_LEN,
        .flags = BB_FIELD_NONE
    },
    {
        .name = "to_user",
        .type = BB_FIELD_STRING,
        .offset = offsetof(Message, to_user),
        .size = APP_USERNAME_LEN,
        .flags = BB_FIELD_NONE
    },
    {
        .name = "body",
        .type = BB_FIELD_STRING,
        .offset = offsetof(Message, body),
        .size = APP_BODY_LEN,
        .flags = BB_FIELD_NONE
    },
    {
        .name = "created_at",
        .type = BB_FIELD_INT,
        .offset = offsetof(Message, created_at),
        .size = sizeof(int),
        .flags = BB_FIELD_NONE
    }
};

bb_schema_t message_schema = {
    .name = "messages",
    .fields = message_fields,
    .field_count = 5,
    .struct_size = sizeof(Message),
    .primary_key_index = 0
};

int message_insert(MessageRepo *repo, Message *msg)
{
    return bb_repo_insert(&repo->base, msg);
}

typedef struct {
    const char *a;
    const char *b;
} conversation_ctx_t;

static int conversation_filter(const void *entity, void *ctx)
{
    const Message *m = entity;
    const conversation_ctx_t *c = ctx;

    if (strcmp(m->from_user, c->a) == 0 && strcmp(m->to_user, c->b) == 0)
        return 1;

    if (strcmp(m->from_user, c->b) == 0 && strcmp(m->to_user, c->a) == 0)
        return 1;

    return 0;
}

static int compare_messages(const void *pa, const void *pb)
{
    const Message *a = pa;
    const Message *b = pb;
    return a->created_at - b->created_at;
}

int message_find_conversation(MessageRepo *repo, const char *a, const char *b, Message **out, size_t *out_count)
{
    conversation_ctx_t ctx = { .a = a, .b = b };

    void *rows = NULL;
    size_t count = 0;

    if (bb_repo_filter(&repo->base, &rows, &count, conversation_filter, &ctx) != 0)
    {
        return -1;
    }

    if (count > 0)
    {
        qsort(rows, count, sizeof(Message), compare_messages);
    }

    *out = rows;
    *out_count = count;

    return 0;
}
