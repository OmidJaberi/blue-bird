#ifndef BB_WEB_ERROR_H
#define BB_WEB_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/error/error.h"

enum {
    BB_ERR_BAD_REQUEST = BB_ERR_WEB + 1,
    BB_ERR_NETWORK,
};


#ifdef __cplusplus
}
#endif

#endif //BB_WEB_ERROR_H
