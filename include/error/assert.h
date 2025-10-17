#ifndef BB_ASSERT_H
#define BB_ASSERT_H

#include <stdio.h>
#include <stdlib.h>

#define BB_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, \
                    "[Blue-Bird ASSERT] %s:%d: %s\n", \
                    __FILE__, __LINE__, msg); \
            abort(); \
        } \
    } while (0)

#endif // BB_ASSERT_H

