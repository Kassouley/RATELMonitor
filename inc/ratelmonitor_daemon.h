#ifndef RATELMONITOR_DAEMON_H
#define RATELMONITOR_DAEMON_H

#include "ratelmonitor_utils.h"

typedef ratelmonitor_status_t (*ratelmonitor_worker_t)(void *user_data);

typedef struct {
    void (*on_exit)(void *user_data);
    void *user_data;
} ratelmonitor_worker_on_exit_t;

ratelmonitor_status_t ratelmonitor_daemon_start(ratelmonitor_worker_t worker,
                                                void *user_data,
                                                ratelmonitor_worker_on_exit_t *on_exit_data);
ratelmonitor_status_t ratelmonitor_daemon_stop(void);

#endif
