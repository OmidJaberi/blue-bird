#ifndef BB_ERROR_H
#define BB_ERROR_H

typedef enum {
	BB_OK = 0,
    BB_ERR_ALLOC,
    BB_ERR_NULL,
    BB_ERR_BAD_REQUEST,
    BB_ERR_NOT_FOUND,
    BB_ERR_INTERNAL,
    BB_ERR_IO,
    BB_ERR_UNKNOWN
} BBErrorCode;

typedef struct {
    BBErrorCode code;
    const char *msg;
} BBError;

#endif // BB_ERROR_H
