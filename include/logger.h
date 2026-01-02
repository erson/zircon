#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

/* Log levels */
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    
    /* Add any additional levels here */
    LOG_LEVEL_COUNT /* Must be last */
} log_level_t;

/* Controls whether to log to console in addition to file */
typedef enum {
    LOG_TO_FILE,       /* Log only to file */
    LOG_TO_CONSOLE,    /* Log only to console */
    LOG_TO_BOTH        /* Log to both file and console */
} log_output_t;

/* Function prototypes */
void log_init(const char *log_file);
void log_set_level(log_level_t level);
void log_set_output(log_output_t output);
void log_write(log_level_t level, const char *fmt, ...);
void log_close(void);

/* Helper macros for convenient logging */
#define LOG_DEBUG(...) log_write(LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...) log_write(LOG_INFO, __VA_ARGS__)
#define LOG_WARN(...) log_write(LOG_WARN, __VA_ARGS__)
#define LOG_ERROR(...) log_write(LOG_ERROR, __VA_ARGS__)
#define LOG_FATAL(...) log_write(LOG_FATAL, __VA_ARGS__)

#endif /* LOGGER_H */ 
