#include "threading.h"


/**
 * Get the current time in milliseconds as a value of type time_t.
 */
time_t get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec) * 1000 + (long) ((tv.tv_usec) / 1000);
}
