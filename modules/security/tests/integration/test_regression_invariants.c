/*
 * Regression guards against the *specific* mistakes this security
 * hardening effort fixed:
 *
 *   - rand()/srand() used anywhere in a security-sensitive path
 *     (Phase 1 replaced this with an OS-backed CSPRNG)
 *   - FNV (or any other non-cryptographic hash) used for password
 *     storage (Phase 2 replaced this with PBKDF2-HMAC-SHA256)
 *   - a hash record claiming to be "sha256"-based without the actual
 *     SHA-256 implementation being wired in behind it (the original
 *     bug this whole effort started from)
 *
 * These are deliberately narrow, mechanical checks over this module's
 * own source text, not general-purpose static analysis - the goal is
 * specifically to make sure Blue-Bird's own past mistakes can't quietly
 * come back in a future edit. They are not a substitute for the
 * correctness tests elsewhere (test_hash.c and test_pbkdf2.c are what
 * actually prove the algorithms compute the right thing, against
 * independently-derived test vectors); this file only proves the
 * *wrong* things aren't there.
 *
 * BB_SECURITY_SRC_DIR is supplied by CMake (see tests/CMakeLists.txt)
 * so this doesn't hardcode an absolute path.
 */

#include <blue-bird/error/assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef BB_SECURITY_SRC_DIR
#error "BB_SECURITY_SRC_DIR must be defined by the build (see tests/CMakeLists.txt)"
#endif

/* Every source file whose randomness must come from the CSPRNG, not
 * rand()/srand(). This is deliberately an explicit, closed list rather
 * than "every .c file in src/" - if a new security-sensitive file is
 * added later, it needs to be added here too for this guard to cover
 * it, the same way it would need to be added to any other reviewed
 * file list. */
static const char *SECURITY_SENSITIVE_FILES[] = {
    "random.c",
    "password.c",
    "password_backend.c",
    "pbkdf2.c",
    "session.c",
    "session_store.c",
    "auth.c",
    "config.c",
};

static char *read_whole_file(const char *path)
{
    FILE *f = fopen(path, "rb");

    if (!f)
        return NULL;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return NULL;
    }

    long size = ftell(f);

    if (size < 0 || fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        return NULL;
    }

    char *buffer = malloc((size_t)size + 1);

    if (!buffer)
    {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buffer, 1, (size_t)size, f);
    fclose(f);

    buffer[read] = '\0';

    return buffer;
}

/*
 * Returns a newly-allocated copy of `source` with the contents of
 * every block comment and line comment replaced with spaces
 * (preserving length/newlines, so line-based reasoning still works if
 * ever added). String and character literals are left alone -
 * deliberately: if "rand(" ever legitimately needs to appear inside a
 * string literal in one of these files, that's unusual enough to be
 * worth this guard flagging for a human to look at, not something to
 * special-case away.
 *
 * This is a minimal, purpose-built stripper for our own source style
 * (no trigraphs, no raw strings) - not a general C preprocessor.
 */
static char *strip_comments(const char *source)
{
    size_t len = strlen(source);
    char *out = malloc(len + 1);

    if (!out)
        return NULL;

    memcpy(out, source, len + 1);

    int in_line_comment = 0;
    int in_block_comment = 0;
    int in_string = 0;
    int in_char = 0;

    for (size_t i = 0; i < len; i++)
    {
        char c = out[i];
        char next = (i + 1 < len) ? out[i + 1] : '\0';

        if (in_line_comment)
        {
            if (c == '\n')
                in_line_comment = 0;
            else
                out[i] = ' ';

            continue;
        }

        if (in_block_comment)
        {
            if (c == '*' && next == '/')
            {
                out[i] = ' ';
                out[i + 1] = ' ';
                in_block_comment = 0;
                i++;
            }
            else if (c != '\n')
            {
                out[i] = ' ';
            }

            continue;
        }

        if (in_string)
        {
            if (c == '\\' && i + 1 < len)
                i++; /* skip escaped char */
            else if (c == '"')
                in_string = 0;

            continue;
        }

        if (in_char)
        {
            if (c == '\\' && i + 1 < len)
                i++;
            else if (c == '\'')
                in_char = 0;

            continue;
        }

        if (c == '/' && next == '/')
        {
            in_line_comment = 1;
            out[i] = ' ';
            continue;
        }

        if (c == '/' && next == '*')
        {
            in_block_comment = 1;
            out[i] = ' ';
            out[i + 1] = ' ';
            i++;
            continue;
        }

        if (c == '"')
        {
            in_string = 1;
            continue;
        }

        if (c == '\'')
        {
            in_char = 1;
            continue;
        }
    }

    return out;
}

static int contains(const char *haystack, const char *needle)
{
    return strstr(haystack, needle) != NULL;
}

void test_no_rand_or_srand_in_security_paths(void)
{
    printf("\tno rand()/srand() calls in any security-sensitive source file...\n");
    fflush(stdout);

    size_t n = sizeof(SECURITY_SENSITIVE_FILES) / sizeof(SECURITY_SENSITIVE_FILES[0]);

    for (size_t i = 0; i < n; i++)
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", BB_SECURITY_SRC_DIR, SECURITY_SENSITIVE_FILES[i]);

        char *source = read_whole_file(path);
        BB_ASSERT(source != NULL); /* the file list itself must be accurate */

        char *code_only = strip_comments(source);
        BB_ASSERT(code_only != NULL);

        int has_rand = contains(code_only, "rand(") || contains(code_only, "srand(");

        if (has_rand)
        {
            fprintf(stderr, "regression: rand()/srand() found in %s\n", path);
        }

        BB_ASSERT(!has_rand);

        free(source);
        free(code_only);
    }
}

void test_no_fnv_password_hashing(void)
{
    printf("\tno FNV (or other legacy) hashing left in the password path...\n");
    fflush(stdout);

    const char *password_files[] = { "password.c", "password_backend.c", "pbkdf2.c" };

    for (size_t i = 0; i < sizeof(password_files) / sizeof(password_files[0]); i++)
    {
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", BB_SECURITY_SRC_DIR, password_files[i]);

        char *source = read_whole_file(path);
        BB_ASSERT(source != NULL);

        /* Case-insensitive search for "fnv" anywhere at all - there is
         * no legitimate reason for this token to appear in the current
         * password implementation, in code or comments. */
        char *lower = malloc(strlen(source) + 1);
        BB_ASSERT(lower != NULL);

        for (size_t j = 0; source[j]; j++)
        {
            lower[j] = (char)tolower((unsigned char)source[j]);
        }
        lower[strlen(source)] = '\0';

        int has_fnv = contains(lower, "fnv");

        if (has_fnv)
        {
            fprintf(stderr, "regression: \"fnv\" found in %s\n", path);
        }

        BB_ASSERT(!has_fnv);

        free(source);
        free(lower);
    }
}

void test_password_records_are_labeled_pbkdf2_sha256(void)
{
    printf("\tpassword records are labeled pbkdf2-sha256, matching the real algorithm...\n");
    fflush(stdout);

    /* This is a narrow, mechanical proxy for "the label matches the
     * implementation" - it confirms the record-format tag is what we
     * expect it to be. It is deliberately NOT a substitute for
     * test_hash.c/test_pbkdf2.c's known-answer tests, which are what
     * actually prove SHA-256/HMAC-SHA256/PBKDF2 compute the right
     * values; a mislabeled-but-differently-broken algorithm could in
     * principle still pass this specific check. */
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", BB_SECURITY_SRC_DIR, "password.c");

    char *password_c = read_whole_file(path);
    BB_ASSERT(password_c != NULL);
    BB_ASSERT(contains(password_c, "bb$pbkdf2-sha256$"));
    free(password_c);

    snprintf(path, sizeof(path), "%s/%s", BB_SECURITY_SRC_DIR, "password_backend.c");

    char *backend_c = read_whole_file(path);
    BB_ASSERT(backend_c != NULL);
    BB_ASSERT(contains(backend_c, "$pbkdf2-sha256$i="));
    free(backend_c);

    /* And the backend must actually be built on the real SHA-256/HMAC
     * primitives, not just claim to be. */
    snprintf(path, sizeof(path), "%s/%s", BB_SECURITY_SRC_DIR, "pbkdf2.c");

    char *pbkdf2_c = read_whole_file(path);
    BB_ASSERT(pbkdf2_c != NULL);
    BB_ASSERT(contains(pbkdf2_c, "bb_hmac_sha256"));
    free(pbkdf2_c);
}

int main(void)
{
    printf("Running security regression invariant checks...\n");
    fflush(stdout);

    test_no_rand_or_srand_in_security_paths();
    test_no_fnv_password_hashing();
    test_password_records_are_labeled_pbkdf2_sha256();

    printf("All tests passed.\n");

    return 0;
}
