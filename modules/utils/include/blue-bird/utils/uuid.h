#ifndef BB_UTILS_UUID_H
#define BB_UTILS_UUID_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stddef.h>
#include <stdint.h>

#define BB_UUID_STR_LEN 36
#define BB_UUID_BUF_LEN 37 /* includes null terminator */

typedef char bb_uuid_t[BB_UUID_BUF_LEN];

/*
 * Generate a random UUID v4 (binary 16-byte form)
 */
int bb_uuid_v4(uint8_t out[16]);

/*
 * Convert binary UUID to string:
 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
 */
int bb_uuid_to_string(const uint8_t uuid[16],
                      char out[BB_UUID_BUF_LEN]);

/*
 * Generate UUID v4 directly as string
 */
int bb_uuid_v4_string(char out[BB_UUID_BUF_LEN]);


#ifdef __cplusplus
}
#endif

#endif //BB_UTILS_UUID_H
