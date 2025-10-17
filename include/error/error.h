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

// Helper Macros
#define BB_SUCCESS() ((BBError){BB_OK, "OK"})
#define BB_ERROR(code, msg) ((BBError){code, msg})
#define BB_FAILED(err) ((err).code != BB_OK)

#endif // BB_ERROR_H
