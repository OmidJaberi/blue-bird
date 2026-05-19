#ifndef BB_RUNTIME_EVENT_H
#define BB_RUNTIME_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif


typedef enum {
    BB_EVENT_READ  = 1 << 0,
    BB_EVENT_WRITE = 1 << 1
} bb_event_type_t;

typedef struct {
    int fd;
    int events;
} bb_poll_event_t;


#ifdef __cplusplus
}
#endif

#endif
