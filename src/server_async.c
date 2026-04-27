#include "server.h"
#include "connection.h"
#include "platform/compat.h"
#include "platform/event_loop.h"
#include "platform/sendfile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct server {
    socket_t sock_fd;
    server_config_t config;
    volatile bool running;
    rate_limiter_t *rate_limiter;
    event_loop_t *event_loop;
    connection_handler_t *conn_handler;
};

static server_t *g_server = NULL;

static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static void signal_handler(int sig) {
    (void)sig;
    if (g_server) {
        g_server->running = false;
    }
}

/* ── Callbacks for connection_handler ─────────────────────────── */

static void on_conn_closed(socket_t fd, void *userdata) {
    (void)fd;
    (void)userdata;
    /* Single-threaded mode: connections are tracked by conn_handler already */
}

/* ── Server Lifecycle ─────────────────────────────────────────── */

server_t *server_create(const server_config_t *config) {
    if (!config) return NULL;

    server_t *server = calloc(1, sizeof(*server));
    if (!server) return NULL;

    server->config = *config;

    unsigned int per_sec = (config->max_requests + 59) / 60;
    if (per_sec == 0) per_sec = 1;

    rate_limit_config_t rate_config = {
        .requests_per_second = per_sec,
        .burst_size = config->max_requests,
        .window_seconds = 60
    };

    server->rate_limiter = rate_limiter_create(&rate_config);
    if (!server->rate_limiter) {
        free(server);
        return NULL;
    }

    /* Adjust config defaults before passing to connection handler */
    server_config_t adjusted = *config;
    if (adjusted.max_request_size == 0) adjusted.max_request_size = 8192;
    if (adjusted.connection_timeout == 0) adjusted.connection_timeout = 30;

    server->conn_handler = connection_handler_create(&adjusted, server->rate_limiter);
    if (!server->conn_handler) {
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }

    event_loop_config_t ev_config = event_loop_default_config();
    server->event_loop = event_loop_create(&ev_config);
    if (!server->event_loop) {
        connection_handler_destroy(server->conn_handler);
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }

    server->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->sock_fd < 0) {
        event_loop_destroy(server->event_loop);
        connection_handler_destroy(server->conn_handler);
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }

    zircon_set_reuseaddr(server->sock_fd);
    zircon_set_reuseport(server->sock_fd);
    zircon_set_nonblocking(server->sock_fd);

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(config->port),
        .sin_addr.s_addr = inet_addr(config->bind_addr)
    };

    if (bind(server->sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server->sock_fd);
        event_loop_destroy(server->event_loop);
        connection_handler_destroy(server->conn_handler);
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }

    if (listen(server->sock_fd, SOMAXCONN) < 0) {
        close(server->sock_fd);
        event_loop_destroy(server->event_loop);
        connection_handler_destroy(server->conn_handler);
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }

    return server;
}

void server_destroy(server_t *server) {
    if (!server) return;

    server->running = false;

    if (server->conn_handler) {
        connection_handler_close_all(server->conn_handler, server->event_loop);
        connection_handler_destroy(server->conn_handler);
    }
    if (server->sock_fd >= 0) close(server->sock_fd);
    if (server->event_loop) event_loop_destroy(server->event_loop);
    if (server->rate_limiter) rate_limiter_destroy(server->rate_limiter);

    if (g_server == server) g_server = NULL;

    free(server);
}

void server_stop(server_t *server) {
    if (server) {
        server->running = false;
    }
}

/* ── Accept Handler ───────────────────────────────────────────── */

static void on_accept(event_loop_t *loop, socket_t fd,
                      event_flags_t events, void *userdata) {
    server_t *server = userdata;
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    (void)events;
    (void)fd;

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    socket_t client_fd = accept(server->sock_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            printf("[%s] Accept failed: %s\n", ts, strerror(errno));
        }
        return;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

    connection_handler_accept(server->conn_handler, loop, client_fd,
                              client_ip, on_conn_closed, NULL);
}

/* ── Event Loop ───────────────────────────────────────────────── */

bool server_run(server_t *server) {
    char ts[32];
    get_timestamp(ts, sizeof(ts));

    if (!server || server->sock_fd < 0) return false;

    server->running = true;
    g_server = server;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    event_loop_add(server->event_loop, server->sock_fd, EVENT_READ, on_accept, server);

    printf("[%s] Server running on http://%s:%d (event-driven, %s)\n",
           ts, server->config.bind_addr, server->config.port,
           event_loop_backend_name(event_loop_get_backend(server->event_loop)));
    printf("[%s] Using %s for file transfers\n", ts, platform_sendfile_method());
    printf("[%s] Keep-Alive: %s, Timeout: %us, Max Request: %u bytes\n",
           ts,
           server->config.keep_alive ? "enabled" : "disabled",
           server->config.connection_timeout,
           server->config.max_request_size);

    while (server->running) {
        int result = event_loop_run_once(server->event_loop, 1000);
        if (result < 0) {
            get_timestamp(ts, sizeof(ts));
            printf("[%s] Event loop error\n", ts);
            break;
        }

        connection_handler_check_timeouts(server->conn_handler, server->event_loop);
    }

    get_timestamp(ts, sizeof(ts));
    printf("\n[%s] Graceful shutdown initiated...\n", ts);

    connection_handler_close_all(server->conn_handler, server->event_loop);

    printf("[%s] All connections closed\n", ts);

    return true;
}
