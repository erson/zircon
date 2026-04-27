#ifndef CONNECTION_H
#define CONNECTION_H

#include "platform/detect.h"
#include "platform/compat.h"
#include "platform/event_loop.h"
#include "rate_limiter.h"
#include "server.h"

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/*
 * Connection Handler
 * ==================
 * Shared HTTP connection state machine used by both single-threaded
 * (server_async.c) and multi-threaded (thread_pool) modes.
 *
 * A connection_handler owns:
 *   - Server configuration (root dir, timeouts, keep-alive)
 *   - Shared rate limiter (mutex-protected, safe across threads)
 *   - Per-handler connection table (indexed by fd)
 *
 * The handler does NOT own the event loop — the caller provides it
 * via connection_handler_accept() so the same handler works with
 * any event loop backend.
 */

typedef struct connection_handler connection_handler_t;

/* Callback invoked when a connection closes (for stats tracking) */
typedef void (*connection_close_cb)(socket_t fd, void *userdata);

/*
 * Create a connection handler.
 * The rate_limiter may be shared across multiple handlers (thread-safe).
 * Takes ownership of root_dir copy.
 */
connection_handler_t *connection_handler_create(const server_config_t *config,
                                                 rate_limiter_t *rate_limiter);

void connection_handler_destroy(connection_handler_t *handler);

/*
 * Accept a new client connection, register it with the event loop.
 * The handler takes ownership of client_fd on success.
 * On failure, client_fd is closed.
 */
void connection_handler_accept(connection_handler_t *handler,
                               event_loop_t *loop,
                               socket_t client_fd,
                               const char *client_ip,
                               connection_close_cb close_cb,
                               void *close_cb_userdata);

/* Check and close idle connections. Call periodically (e.g., every second). */
void connection_handler_check_timeouts(connection_handler_t *handler,
                                       event_loop_t *loop);

/* Close all connections immediately (for graceful shutdown). */
void connection_handler_close_all(connection_handler_t *handler,
                                  event_loop_t *loop);

/* Get the rate limiter (for configuration queries). */
rate_limiter_t *connection_handler_get_rate_limiter(connection_handler_t *handler);

#endif /* CONNECTION_H */
