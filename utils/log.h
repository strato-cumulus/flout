#ifndef FLOUT_UTIL__LOG_H_INCLUDED
#define FLOUT_UTIL__LOG_H_INCLUDED

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

enum log_level {
    ERROR,
    WARN,
    INFO,
    DEBUG
};

const char * log_level_to_string(enum log_level log_level);

void log_message(enum log_level log_level, const char *caller_name, const char *format, ...);

#endif
