#include "log.h"


/**
 * Convert log levels to their textual representation.
 */
const char * log_level_to_string(enum log_level log_level)
{
    switch (log_level) {
    case ERROR:
        return "ERROR";
    case WARN:
        return "WARN";
    case INFO:
        return "INFO";
    case DEBUG:
        return "DEBUG";
    }
    return "UNKNOWN";
}


/**
 * Log a message with severity of log_level as specified by format and args.
 * caller_name should be a string identifying the source of the log (e.g. function name).
 */
void log_message(enum log_level log_level, const char *caller_name, const char *format, ...)
{
    time_t timer;
    char time_buffer[24];
    struct tm *tm_info;
    struct timeval tv;

    timer = time(NULL);
    tm_info = localtime(&timer);
    strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    // ANSI C does not have millisecond precision; use a different API and append
    time_buffer[19] = '.';
    gettimeofday(&tv, NULL);
    snprintf(&time_buffer[20], 4, "%d", tv.tv_usec);

    const char *log_level_string = log_level_to_string(log_level);
    printf("%s %s %s: ", time_buffer, log_level_string, caller_name);
    va_list argp;
    va_start(argp, format);
    vprintf(format, argp);
    va_end(argp);
    printf("\n");
}
