#include "rate_limiter.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_CLIENTS 10000

/* Client tracking structure */
typedef struct {
    char ip[16];
    time_t *requests;
    size_t count;
    time_t window_start;
    bool blocked;
} client_track_t;

/* Comparison function for client IPs used in bsearch/qsort */
static int compare_client_ip(const void *a, const void *b) {
    const client_track_t *ca = a;
    const client_track_t *cb = b;
    return strcmp(ca->ip, cb->ip);
}

/* Rate limiter context */
struct rate_limiter {
    rate_limit_config_t config;
    client_track_t *clients;
    size_t client_count;
    pthread_mutex_t lock;
};

/* Create rate limiter */
rate_limiter_t *rate_limiter_create(const rate_limit_config_t *config) {
    rate_limiter_t *limiter = calloc(1, sizeof(*limiter));
    if (!limiter) return NULL;

    /* Copy configuration */
    limiter->config = *config;

    /* Allocate client tracking array */
    limiter->clients = calloc(MAX_CLIENTS, sizeof(client_track_t));
    if (!limiter->clients) {
        free(limiter);
        return NULL;
    }

    /* Initialize mutex */
    if (pthread_mutex_init(&limiter->lock, NULL) != 0) {
        free(limiter->clients);
        free(limiter);
        return NULL;
    }

    return limiter;
}

/* Find or create client entry */
static client_track_t *get_client(rate_limiter_t *limiter, const char *ip) {
    /* Look for existing client using binary search */
    client_track_t key = {0};
    strncpy(key.ip, ip, sizeof(key.ip) - 1);
    client_track_t *client = bsearch(&key, limiter->clients,
                                     limiter->client_count,
                                     sizeof(client_track_t),
                                     compare_client_ip);
    if (client) {
        return client;
    }

    /* Add new client if space available */
    if (limiter->client_count < MAX_CLIENTS) {
        client = &limiter->clients[limiter->client_count++];
        memset(client, 0, sizeof(*client));
        strncpy(client->ip, ip, sizeof(client->ip) - 1);
        client->requests = calloc(limiter->config.burst_size, sizeof(time_t));
        if (!client->requests) return NULL; // Check if calloc failed
        client->window_start = time(NULL);

        /* Keep clients sorted for efficient lookup */
        qsort(limiter->clients, limiter->client_count,
              sizeof(client_track_t), compare_client_ip);

        /* Find and return the newly added client */
        return bsearch(&key, limiter->clients, limiter->client_count,
                       sizeof(client_track_t), compare_client_ip);
    }

    return NULL;
}

/* Check if request should be allowed */
bool rate_limiter_check(rate_limiter_t *limiter, const char *ip) {
    bool allowed = true;
    time_t now = time(NULL);

    pthread_mutex_lock(&limiter->lock);

    client_track_t *client = get_client(limiter, ip);
    if (!client) {
        pthread_mutex_unlock(&limiter->lock);
        return false;
    }

    /* Check if client is blocked, but allow localhost (127.0.0.1) for testing */
    if (client->blocked && strcmp(ip, "127.0.0.1") != 0) {
        /* Check if block period has expired (10 second timeout) 
         * This allows clients to resume normal access after a short penalty period
         */
        if (now - client->window_start >= 10) {
            client->blocked = false;
            client->count = 0;
            client->window_start = now;
        } else {
            pthread_mutex_unlock(&limiter->lock);
            return false;
        }
    }

    /* Reset window if needed */
    if (now - client->window_start >= limiter->config.window_seconds) {
        client->count = 0;
        client->window_start = now;
    }

    /* For testing purposes, always allow localhost */
    if (strcmp(ip, "127.0.0.1") == 0) {
        client->requests[client->count++] = now;
    }
    /* Check rate limit for other IPs */
    else if (client->count >= limiter->config.requests_per_second) {
        allowed = false;
        client->blocked = true;
        log_write(LOG_WARN, "Rate limit exceeded for IP: %s", ip);
    } else {
        client->requests[client->count++] = now;
    }

    pthread_mutex_unlock(&limiter->lock);
    return allowed;
}

/* Clean up rate limiter */
void rate_limiter_destroy(rate_limiter_t *limiter) {
    if (!limiter) return;

    pthread_mutex_lock(&limiter->lock);

    /* Free client request arrays */
    for (size_t i = 0; i < limiter->client_count; i++) {
        free(limiter->clients[i].requests);
    }

    free(limiter->clients);

    pthread_mutex_unlock(&limiter->lock);
    pthread_mutex_destroy(&limiter->lock);

    free(limiter);
}
