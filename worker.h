#ifndef FLOUT_WORKER_H_INCLUDED
#define FLOUT_WORKER_H_INCLUDED

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rpc/command.h"
#include "utils/err.h"
#include "utils/log.h"
#include "utils/net.h"
#include "utils/threading.h"

typedef struct {
    struct timeval interval;
} flout_worker_heartbeat_fn_params;

#endif
