#ifndef BB_MIGRATE_ERR_H
#define BB_MIGRATE_ERR_H

/* ---------------------------
 * bb-migrate error reporting
 *
 * Every layer reports failures the same way: return non-zero and (when
 * the caller passed one) fill a bb_migrate_err_t with a human-readable
 * message. Messages are meant to be printed as-is by the CLI, so they
 * name the migration/file involved rather than leaking internals.
 * Passing NULL for `err` is always allowed.
 * --------------------------- */

#define BB_MIGRATE_ERR_MAX 512

typedef struct {
    char msg[BB_MIGRATE_ERR_MAX];
} bb_migrate_err_t;

/* printf-style; safe to call with err == NULL. */
void bb_migrate_err_set(bb_migrate_err_t *err, const char *fmt, ...);

#endif //BB_MIGRATE_ERR_H
