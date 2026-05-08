#include "error/error.h"

static const char *error_strings[] = {
    [BB_OK] = "OK",
    [BB_ERR_ALLOC] = "Memory allocation failed",
    [BB_ERR_NULL] = "Null pointer dereference",
    [BB_ERR_BAD_REQUEST] = "Bad request",
    [BB_ERR_NOT_FOUND] = "Resource not found",
    [BB_ERR_INTERNAL] = "Internal server error",
    [BB_ERR_IO] = "I/O error",
    [BB_ERR_UNKNOWN] = "Unknown error"
};

const char *bb_strerror(BBErrorCode code)
{
    if (code < 0 || code >= (int)(sizeof(error_strings) / sizeof(error_strings[0])))
        return "Invalid error code";
    return error_strings[code] ? error_strings[code] : "Unspecified error";
}
