#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <stdbool.h>
#include <time.h>

/* Rate limiter context */
typedef struct rate_limiter rate_limiter_t;

/* Rate limit configuration */
typedef struct {
    unsigned int requests_per_second;
    unsigned int burst_size;
    time_t window_seconds;
} rate_limit_config_t;

/* Function prototypes */
rate_limiter_t *rate_limiter_create(const rate_limit_config_t *config);
void rate_limiter_destroy(rate_limiter_t *limiter);
bool rate_limiter_check(rate_limiter_t *limiter, const char *ip);

#endif /* RATE_LIMITER_H */
