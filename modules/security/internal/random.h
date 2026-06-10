#ifndef BB_SECURITY_RANDOM_H
#define BB_SECURITY_RANDOM_H

#include <stddef.h>

#include "blue-bird/error/error.h"

bb_error_t _bb_random_bytes(void *buffer, size_t size);

bb_error_t _bb_random_hex(char *buffer, size_t hex_size);

#endif
