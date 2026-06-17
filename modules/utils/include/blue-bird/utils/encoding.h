#ifndef BB_UTILS_ENCODING_H
#define BB_UTILS_ENCODING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

void bb_decode_percent(char *s, int decode_plus);

char *bb_base64_encode(const void *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif //BB_UTILS_ENCODING_H
