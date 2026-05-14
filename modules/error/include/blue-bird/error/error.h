#ifndef BB_ERROR_H
#define BB_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    BB_OK = 0,
    BB_ERR_ALLOC,
    BB_ERR_NULL,
    BB_ERR_BAD_REQUEST,
    BB_ERR_NOT_FOUND,
    BB_ERR_INTERNAL,
    BB_ERR_IO,
    BB_ERR_UNKNOWN
} bb_error_code_t;

typedef struct {
    bb_error_code_t code;
    const char *msg;
} bb_error_t;

// Helper Macros
#define BB_SUCCESS() ((bb_error_t){BB_OK, "OK"})
#define BB_ERROR(code, msg) ((bb_error_t){code, msg})
#define BB_FAILED(err) ((err).code != BB_OK)

const char *bb_strerror(bb_error_code_t code);


#ifdef __cplusplus
}
#endif

#endif //BB_ERROR_H
