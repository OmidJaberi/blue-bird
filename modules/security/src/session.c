#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "../internal/random.h"
#include "../internal/session_store.h"

typedef struct
{
    bb_session_t *sessions;
    size_t count;
    size_t capacity;
} bb_memory_store_t;

static bb_memory_store_t store = {0};

static bb_error_t ensure_capacity(void)
{
    if (store.count < store.capacity)
        return BB_SUCCESS();

    size_t new_cap = store.capacity == 0 ? 32 : store.capacity * 2;

    bb_session_t *tmp = realloc(store.sessions, new_cap * sizeof(bb_session_t));

    if (!tmp)
        return BB_ERROR(BB_ERR_ALLOC, "allocation failed");

    store.sessions = tmp;
    store.capacity = new_cap;

    return BB_SUCCESS();
}

// Store
bb_error_t _bb_session_store_create(const bb_session_t *session)
{
    bb_error_t err = ensure_capacity();

    if (BB_FAILED(err))
        return err;

    store.sessions[store.count++] = *session;

    return BB_SUCCESS();
}

bb_error_t _bb_session_store_get(const char *id, bb_session_t *session)
{
    for (size_t i = 0; i < store.count; i++)
    {
        if (strcmp(store.sessions[i].id, id) == 0)
        {
            *session = store.sessions[i];

            return BB_SUCCESS();
        }
    }

    return BB_ERROR(BB_ERR_NOT_FOUND, "session not found");
}

bb_error_t _bb_session_store_destroy(const char *id)
{
    for (size_t i = 0; i < store.count; i++)
    {
        if (strcmp(store.sessions[i].id, id) == 0)
        {
            store.sessions[i] = store.sessions[store.count - 1];
            store.count--;
            return BB_SUCCESS();
        }
    }

    return BB_ERROR(BB_ERR_NOT_FOUND, "session not found");
}

//Clean-up
bb_error_t _bb_session_store_cleanup(void)
{
    time_t now = time(NULL);

    size_t j = 0;

    for (size_t i = 0; i < store.count; i++)
    {
        if (store.sessions[i].expires_at > now)
        {
            store.sessions[j++] = store.sessions[i];
        }
    }

    store.count = j;

    return BB_SUCCESS();
}

//Public API
bb_error_t bb_session_create(const char *user_id, time_t ttl, bb_session_t *session)
{
    if (!user_id || !session)
        return BB_ERROR(BB_ERR_NULL, "null argument");

    memset(session, 0, sizeof(*session));

    strncpy(session->user_id, user_id, sizeof(session->user_id)-1);

    _bb_random_hex(session->id, sizeof(session->id));

    session->expires_at = time(NULL) + ttl;

    return _bb_session_store_create(session);
}

bb_error_t bb_session_get(const char *id, bb_session_t *session)
{
    bb_error_t err = _bb_session_store_get(id, session);

    if (BB_FAILED(err))
        return err;

    if (session->expires_at < time(NULL))
    {
        return BB_ERROR(BB_ERR_SESSION_EXPIRED, "expired");
    }

    return BB_SUCCESS();
}

bb_error_t bb_session_destroy(const char *id)
{
    return _bb_session_store_destroy(id);
}
