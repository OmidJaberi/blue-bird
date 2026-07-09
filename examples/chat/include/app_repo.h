#ifndef APP_REPO_H
#define APP_REPO_H

#include <blue-bird/persist/model.h>
#include <blue-bird/persist/repo.h>
#include <blue-bird/utils/uuid.h>
#include <blue-bird/security/password.h>

#include <stddef.h>

#define APP_USERNAME_LEN 64
#define APP_BODY_LEN 2048

/* ---- User ---- */

typedef struct {
    char username[APP_USERNAME_LEN];
    char password_hash[BB_PASSWORD_HASH_MAX];
} User;

typedef struct {
    bb_repo_t base;
} UserRepo;

extern UserRepo global_user_repo;

extern bb_field_t user_fields[];
extern bb_schema_t user_schema;

int user_insert(UserRepo *repo, User *user);
int user_find_by_username(UserRepo *repo, User *out, const char *username);
int user_exists(UserRepo *repo, const char *username);

/* ---- Message ---- */

typedef struct {
    bb_uuid_t id;
    char from_user[APP_USERNAME_LEN];
    char to_user[APP_USERNAME_LEN];
    char body[APP_BODY_LEN];
    int created_at;
} Message;

typedef struct {
    bb_repo_t base;
} MessageRepo;

extern MessageRepo global_message_repo;

extern bb_field_t message_fields[];
extern bb_schema_t message_schema;

int message_insert(MessageRepo *repo, Message *msg);

/*
 * Fetches the conversation between "a" and "b", sorted by created_at
 * ascending. Caller must free() *out on success (when *out_count > 0).
 */
int message_find_conversation(MessageRepo *repo, const char *a, const char *b, Message **out, size_t *out_count);

#endif //APP_REPO_H
