#ifndef FLOUT_COORDINATOR_H_INCLUDED
#define FLOUT_COORDINATOR_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/err.h"
#include "utils/log.h"
#include "utils/net.h"
#include "utils/threading.h"

#define MAX_CONNECTED_WORKERS 8

typedef struct {
    int status;
    int socket_fd;
    suseconds_t last_activity_ts;
} flout_worker_slot_t;

#define SFLOUT_FREE 0
#define SFLOUT_OCCUPIED 1

#endif
