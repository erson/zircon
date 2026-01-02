#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* Logger state */
static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static log_level_t current_level = LOG_INFO;
static log_output_t output_mode = LOG_TO_BOTH;

/* Level strings */
static const char *level_strings[] = {
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"
};

/* ANSI color codes for console output */
static const char *level_colors[] = {
    "\033[36m",  /* Cyan for DEBUG */
    "\033[32m",  /* Green for INFO */
    "\033[33m",  /* Yellow for WARN */
    "\033[31m",  /* Red for ERROR */
    "\033[35m"   /* Magenta for FATAL */
};

static const char *reset_color = "\033[0m";

/* Set log level */
void log_set_level(log_level_t level) {
    if (level < LOG_LEVEL_COUNT) {
        current_level = level;
    }
}

/* Set output mode */
void log_set_output(log_output_t output) {
    output_mode = output;
}

/* Initialize logger */
void log_init(const char *filename) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_file && log_file != stderr) {
        fclose(log_file);
        log_file = NULL;
    }
    
    if (filename) {
        log_file = fopen(filename, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file %s, falling back to stderr\n", filename);
            log_file = stderr;
        }
    } else {
        log_file = stderr;
    }
    
    pthread_mutex_unlock(&log_mutex);
    
    /* Log startup */
    log_write(LOG_INFO, "Logger initialized");
}

/* Write log message */
void log_write(log_level_t level, const char *fmt, ...) {
    /* Check if this level should be logged */
    if (level < current_level) {
        return;
    }
    
    /* Ensure log_file is valid */
    if (!log_file) {
        log_file = stderr;
    }

    /* Get current time */
    time_t now = time(NULL);
    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    /* Format message */
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    /* Write log entry */
    pthread_mutex_lock(&log_mutex);
    
    /* Log to file if needed */
    if (output_mode == LOG_TO_FILE || output_mode == LOG_TO_BOTH) {
        if (log_file != stderr) {
            fprintf(log_file, "[%s] %s: %s\n", time_str, level_strings[level], message);
            fflush(log_file);
        }
    }
    
    /* Log to console if needed */
    if (output_mode == LOG_TO_CONSOLE || output_mode == LOG_TO_BOTH || log_file == stderr) {
        /* Use colors for console output */
        if (level < LOG_LEVEL_COUNT) {
            fprintf(stderr, "%s[%s] %s: %s%s\n", 
                    level_colors[level], time_str, level_strings[level], 
                    message, reset_color);
        } else {
            fprintf(stderr, "[%s] UNKNOWN: %s\n", time_str, message);
        }
    }
    
    pthread_mutex_unlock(&log_mutex);
}

/* Close logger */
void log_close(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_file && log_file != stderr) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
} 
