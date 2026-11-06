#ifndef BB_PERSIST_TEXT_COPY_H
#define BB_PERSIST_TEXT_COPY_H

#include <stddef.h>
#include <string.h>

/* Copies `text` into the fixed-size char field `dst` (`size` bytes).
 *
 * The whole field is always initialised and the result is always
 * NUL-terminated: a value that does not fit is truncated to size - 1 bytes,
 * without cutting a UTF-8 character in half, and a NULL `text` gives an empty
 * string. strncpy() cannot be used for this: it leaves a value of `size` bytes
 * or more unterminated.
 *
 * Shared by everything that turns stored text back into an entity (JSON,
 * SQLite, PostgreSQL), so they all agree on what "does not fit" means. */
static inline void bb_persist_copy_text_field(char *dst, size_t size, const char *text)
{
    if (size == 0)
        return;

    size_t len = text ? strlen(text) : 0;
    size_t n = len < size ? len : size - 1;

    if (n < len)
    {
        /* text[n] is the first byte left out; if it is a UTF-8 continuation
         * byte the cut is inside a character, so step back to its start. */
        while (n > 0 && ((unsigned char)text[n] & 0xC0) == 0x80)
            n--;
    }

    memset(dst, 0, size);
    if (n > 0)
        memcpy(dst, text, n);
}

#endif /* BB_PERSIST_TEXT_COPY_H */
