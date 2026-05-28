#ifndef BB_POLLER_H
#define BB_POLLER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "blue-bird/runtime/event.h"

typedef struct bb_poller bb_poller_t;

bb_poller_t *bb_poller_create(void);

void bb_poller_destroy(bb_poller_t *poller);

int bb_poller_register(bb_poller_t *poller, int fd, int events);

int bb_poller_unregister(bb_poller_t *poller, int fd);

int bb_poller_wait(bb_poller_t *poller, bb_poll_event_t *events, int max_events, int timeout_ms);


#ifdef __cplusplus
}
#endif

#endif
