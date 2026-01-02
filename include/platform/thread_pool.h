#ifndef PLATFORM_THREAD_POOL_H
#define PLATFORM_THREAD_POOL_H

#include "platform/detect.h"
#include "platform/compat.h"
#include "platform/event_loop.h"

/**
 * Thread Pool Architecture
 * ========================
 * 
 * Each worker thread owns:
 * - Its own event loop (kqueue/epoll)
 * - Its own listen socket (via SO_REUSEPORT)
 * - Its own connection pool
 * 
 * Benefits:
 * - No lock contention between workers
 * - Kernel distributes connections across workers
 * - CPU cache locality (thread pinned to core)
 * - Linear scalability with core count
 * 
 * ┌─────────────────────────────────────────────────────────┐
 * │                    Main Thread                          │
 * │  - Creates worker threads                               │
 * │  - Handles signals (SIGINT, SIGTERM)                    │
 * │  - Coordinates graceful shutdown                        │
 * └─────────────────────────────────────────────────────────┘
 *                           │
 *          ┌────────────────┼────────────────┐
 *          ▼                ▼                ▼
 * ┌─────────────────┐ ┌─────────────────┐ ┌─────────────────┐
 * │   Worker 0      │ │   Worker 1      │ │   Worker N      │
 * │  ┌───────────┐  │ │  ┌───────────┐  │ │  ┌───────────┐  │
 * │  │ Listen FD │  │ │  │ Listen FD │  │ │  │ Listen FD │  │
 * │  │ (REUSEPORT)│  │ │  │ (REUSEPORT)│  │ │  │ (REUSEPORT)│  │
 * │  └───────────┘  │ │  └───────────┘  │ │  └───────────┘  │
 * │  ┌───────────┐  │ │  ┌───────────┐  │ │  ┌───────────┐  │
 * │  │Event Loop │  │ │  │Event Loop │  │ │  │Event Loop │  │
 * │  │ (kqueue)  │  │ │  │ (kqueue)  │  │ │  │ (kqueue)  │  │
 * │  └───────────┘  │ │  └───────────┘  │ │  └───────────┘  │
 * │  CPU Core 0     │ │  CPU Core 1     │ │  CPU Core N     │
 * └─────────────────┘ └─────────────────┘ └─────────────────┘
 */

typedef struct worker worker_t;
typedef struct thread_pool thread_pool_t;

/**
 * Worker callback - called for each new connection
 * The callback owns the connection lifecycle
 */
typedef void (*worker_accept_cb)(worker_t *worker, socket_t client_fd,
                                  struct sockaddr_in *client_addr, void *userdata);

/**
 * Worker configuration
 */
typedef struct {
    const char *bind_addr;      /* Address to bind (e.g., "0.0.0.0") */
    uint16_t port;              /* Port to listen on */
    int backlog;                /* Listen backlog */
    int max_connections;        /* Max connections per worker */
    worker_accept_cb on_accept; /* Connection accept callback */
    void *userdata;             /* User data passed to callbacks */
} worker_config_t;

/**
 * Thread pool configuration
 */
typedef struct {
    unsigned int num_workers;   /* Number of worker threads (0 = auto-detect) */
    bool pin_to_cpu;            /* Pin each worker to a CPU core */
    worker_config_t worker;     /* Per-worker configuration */
} thread_pool_config_t;

/**
 * Worker statistics
 */
typedef struct {
    uint64_t connections_accepted;
    uint64_t connections_active;
    uint64_t bytes_received;
    uint64_t bytes_sent;
    uint64_t requests_processed;
} worker_stats_t;

/**
 * Get worker's event loop (for adding client sockets)
 */
ZIRCON_API event_loop_t *worker_get_event_loop(worker_t *worker);

/**
 * Get worker's ID (0 to num_workers-1)
 */
ZIRCON_API unsigned int worker_get_id(worker_t *worker);

/**
 * Get worker's statistics
 */
ZIRCON_API void worker_get_stats(worker_t *worker, worker_stats_t *stats);

/**
 * Increment worker statistics (thread-safe within the worker)
 */
ZIRCON_API void worker_stat_connection_accepted(worker_t *worker);
ZIRCON_API void worker_stat_connection_closed(worker_t *worker);
ZIRCON_API void worker_stat_bytes_received(worker_t *worker, size_t bytes);
ZIRCON_API void worker_stat_bytes_sent(worker_t *worker, size_t bytes);
ZIRCON_API void worker_stat_request_processed(worker_t *worker);

/**
 * Create thread pool with given configuration
 * Returns NULL on failure
 */
ZIRCON_API thread_pool_t *thread_pool_create(const thread_pool_config_t *config);

/**
 * Start all worker threads
 * Returns 0 on success, -1 on failure
 */
ZIRCON_API int thread_pool_start(thread_pool_t *pool);

/**
 * Stop all worker threads gracefully
 * Waits for in-flight requests to complete
 */
ZIRCON_API void thread_pool_stop(thread_pool_t *pool);

/**
 * Destroy thread pool and free resources
 * Calls thread_pool_stop() if still running
 */
ZIRCON_API void thread_pool_destroy(thread_pool_t *pool);

/**
 * Get number of workers in the pool
 */
ZIRCON_API unsigned int thread_pool_num_workers(thread_pool_t *pool);

/**
 * Get aggregated statistics from all workers
 */
ZIRCON_API void thread_pool_get_stats(thread_pool_t *pool, worker_stats_t *stats);

/**
 * Check if thread pool is running
 */
ZIRCON_API bool thread_pool_is_running(thread_pool_t *pool);

/**
 * Default configuration helper
 */
static inline thread_pool_config_t thread_pool_default_config(void) {
    thread_pool_config_t config = {
        .num_workers = 0,           /* Auto-detect CPU count */
        .pin_to_cpu = true,
        .worker = {
            .bind_addr = "0.0.0.0",
            .port = 8000,
            .backlog = 1024,
            .max_connections = 10000,
            .on_accept = NULL,
            .userdata = NULL
        }
    };
    return config;
}

#endif /* PLATFORM_THREAD_POOL_H */
