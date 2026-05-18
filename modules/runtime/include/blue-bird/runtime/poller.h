#ifndef BB_POLLER_H
#define BB_POLLER_H


#ifdef __cplusplus
extern "C" {
#endif

typedef struct bb_poller bb_poller_t;

bb_poller_t *bb_poller_create(void);

void bb_poller_destroy(
    bb_poller_t *poller
);

void bb_poller_poll(
    bb_poller_t *poller
);

#ifdef __cplusplus
}
#endif


#endif
