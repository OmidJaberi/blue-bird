#ifndef BB_PASSWORD_BACKEND_H
#define BB_PASSWORD_BACKEND_H

#include <stdlib.h>
#include "blue-bird/error/error.h"

bb_error_t _bb_password_backend_hash(const char *password, const char *salt, char *output, size_t output_size);

#endif
