#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.h"
#include "connection.h"
#include "platform/detect.h"
#include "platform/thread_pool.h"

void platform_detect_init(void);
void platform_print_info(void);
int platform_has_sendfile_support(void);
const char *platform_sendfile_method(void);
const char *platform_get_event_backend_name(void);
unsigned int platform_cpu_count(void);

static volatile sig_atomic_t g_shutdown = 0;
static thread_pool_t *g_pool = NULL;

/* Shared rate limiter across all workers (thread-safe) */
static rate_limiter_t *g_shared_rate_limiter = NULL;

static void get_timestamp_str(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static void print_platform_info(void) {
    platform_detect_init();
    platform_print_info();
    printf("sendfile:     %s (%s)\n", 
           platform_has_sendfile_support() ? "yes" : "no",
           platform_sendfile_method());
    printf("\n");
}

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("Options:\n");
    printf("  --config FILE      Load config from FILE\n");
    printf("  --platform-info    Show platform capabilities\n");
    printf("  --workers N        Run with N worker threads (0=auto, default=single-threaded)\n");
    printf("  --port PORT        Listen on PORT (default: 8000)\n");
    printf("  --bind ADDR        Bind to ADDR (default: 127.0.0.1)\n");
    printf("  --root DIR         Serve files from DIR (default: www)\n");
    printf("  --timeout SEC      Connection timeout in seconds (default: 30)\n");
    printf("  --keep-alive       Enable HTTP Keep-Alive\n");
    printf("  --help             Show this help\n");
}

static bool load_config_file(const char *path, int *port, char *bind_addr, size_t bind_size,
                              char *root_dir, size_t root_size, unsigned int *timeout,
                              bool *keep_alive) {
    FILE *f = fopen(path, "r");
    if (!f) return false;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;
        
        char *eq = strchr(p, '=');
        if (!eq) continue;
        
        *eq = '\0';
        char *key = p;
        char *val = eq + 1;
        
        while (*key && (key[strlen(key)-1] == ' ' || key[strlen(key)-1] == '\t'))
            key[strlen(key)-1] = '\0';
        while (*val == ' ' || *val == '\t') val++;
        char *nl = strchr(val, '\n');
        if (nl) *nl = '\0';
        
        if (strcmp(key, "port") == 0) {
            *port = atoi(val);
        } else if (strcmp(key, "bind") == 0) {
            strncpy(bind_addr, val, bind_size - 1);
            bind_addr[bind_size - 1] = '\0';
        } else if (strcmp(key, "root") == 0) {
            strncpy(root_dir, val, root_size - 1);
            root_dir[root_size - 1] = '\0';
        } else if (strcmp(key, "timeout") == 0) {
            *timeout = (unsigned int)atoi(val);
        } else if (strcmp(key, "keep_alive") == 0) {
            *keep_alive = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
        }
    }
    
    fclose(f);
    return true;
}

static void signal_handler(int sig) {
    (void)sig;
    g_shutdown = 1;
    if (g_pool) {
        thread_pool_stop(g_pool);
    }
}

/* ── Multi-Threaded Worker Data ───────────────────────────────── */

typedef struct {
    server_config_t config;
    rate_limiter_t *rate_limiter;
} worker_context_t;

#define MAX_WORKERS 64
static connection_handler_t *g_worker_handlers[MAX_WORKERS];

static void on_worker_conn_closed(socket_t fd, void *userdata) {
    worker_t *worker = (worker_t *)userdata;
    (void)fd;
    worker_stat_connection_closed(worker);
}

static void on_worker_periodic(worker_t *worker, void *userdata) {
    (void)userdata;
    unsigned int wid = worker_get_id(worker);
    if (wid >= MAX_WORKERS) return;
    connection_handler_t *handler = g_worker_handlers[wid];
    if (handler) {
        event_loop_t *loop = worker_get_event_loop(worker);
        connection_handler_check_timeouts(handler, loop);
    }
}

static void on_worker_accept(worker_t *worker, socket_t client_fd,
                              struct sockaddr_in *client_addr, void *userdata) {
    worker_context_t *ctx = (worker_context_t *)userdata;

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));

    event_loop_t *loop = worker_get_event_loop(worker);
    if (!loop) {
        close(client_fd);
        return;
    }

    /* Lazily create per-worker connection handler on first accept */
    unsigned int wid = worker_get_id(worker);
    if (wid >= MAX_WORKERS) {
        close(client_fd);
        return;
    }
    if (!g_worker_handlers[wid]) {
        g_worker_handlers[wid] = connection_handler_create(&ctx->config, ctx->rate_limiter);
        if (!g_worker_handlers[wid]) {
            close(client_fd);
            return;
        }
    }

    worker_stat_connection_accepted(worker);

    connection_handler_accept(g_worker_handlers[wid], loop, client_fd,
                              client_ip, on_worker_conn_closed, worker);

    worker_stat_request_processed(worker);
}

static int run_multi_threaded(int port, const char *bind_addr, const char *root_dir,
                               unsigned int timeout, bool keep_alive,
                               unsigned int num_workers) {
    char ts[32];
    get_timestamp_str(ts, sizeof(ts));
    printf("[%s] Multi-threaded mode: %u workers\n", ts, 
           num_workers == 0 ? platform_cpu_count() : num_workers);
    
    /* Create server config for per-worker handlers */
    server_config_t config = {
        .port = port,
        .bind_addr = {0},
        .root_dir = {0},
        .max_requests = 100,
        .connection_timeout = timeout,
        .max_request_size = 8192,
        .keep_alive = keep_alive
    };
    strncpy(config.bind_addr, bind_addr, sizeof(config.bind_addr) - 1);
    strncpy(config.root_dir, root_dir, sizeof(config.root_dir) - 1);

    /* Create shared rate limiter (thread-safe, mutex-protected) */
    unsigned int per_sec = (config.max_requests + 59) / 60;
    if (per_sec == 0) per_sec = 1;

    rate_limit_config_t rate_config = {
        .requests_per_second = per_sec,
        .burst_size = config.max_requests,
        .window_seconds = 60
    };
    g_shared_rate_limiter = rate_limiter_create(&rate_config);
    if (!g_shared_rate_limiter) {
        get_timestamp_str(ts, sizeof(ts));
        fprintf(stderr, "[%s] Error: Failed to create rate limiter\n", ts);
        return 1;
    }

    /* Each worker lazily creates its own connection_handler via on_worker_accept */
    memset(g_worker_handlers, 0, sizeof(g_worker_handlers));

    /* Per-worker context (config + shared rate_limiter) */
    worker_context_t worker_ctx = {
        .config = config,
        .rate_limiter = g_shared_rate_limiter
    };

    /* Create thread pool */
    thread_pool_config_t pool_config = thread_pool_default_config();
    pool_config.num_workers = num_workers;
    pool_config.pin_to_cpu = true;
    pool_config.worker.port = port;
    pool_config.worker.bind_addr = bind_addr;
    pool_config.worker.backlog = 1024;
    pool_config.worker.max_connections = 10000;
    pool_config.worker.on_accept = on_worker_accept;
    pool_config.worker.on_periodic = on_worker_periodic;
    pool_config.worker.userdata = &worker_ctx;
    
    g_pool = thread_pool_create(&pool_config);
    if (!g_pool) {
        get_timestamp_str(ts, sizeof(ts));
        fprintf(stderr, "[%s] Error: Failed to create thread pool\n", ts);
        rate_limiter_destroy(g_shared_rate_limiter);
        g_shared_rate_limiter = NULL;
        return 1;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (thread_pool_start(g_pool) < 0) {
        get_timestamp_str(ts, sizeof(ts));
        fprintf(stderr, "[%s] Error: Failed to start thread pool\n", ts);
        thread_pool_destroy(g_pool);
        g_pool = NULL;
        rate_limiter_destroy(g_shared_rate_limiter);
        g_shared_rate_limiter = NULL;
        return 1;
    }
    
    printf("[%s] Server Configuration:\n", ts);
    printf("  Listening: http://%s:%d\n", bind_addr, port);
    printf("  Root:      %s\n", root_dir);
    printf("  Timeout:   %u seconds\n", timeout);
    printf("  KeepAlive: %s\n", keep_alive ? "enabled" : "disabled");
    printf("  Workers:   %u (SO_REUSEPORT, per-worker connection pools)\n",
           pool_config.num_workers);
    printf("\n");
    printf("[%s] Server running on http://%s:%d (multi-threaded)\n",
           ts, bind_addr, port);
    
    /* Main thread: monitor stats */
    while (!g_shutdown) {
        sleep(1);
        
        worker_stats_t stats;
        thread_pool_get_stats(g_pool, &stats);
        
        static uint64_t last_requests = 0;
        uint64_t rps = stats.requests_processed - last_requests;
        last_requests = stats.requests_processed;
        
        if (rps > 0) {
            get_timestamp_str(ts, sizeof(ts));
            printf("[%s] Stats: %llu active, %llu total, %llu req/s\n",
                   ts,
                   (unsigned long long)stats.connections_active,
                   (unsigned long long)stats.connections_accepted,
                   (unsigned long long)rps);
        }
    }
    
    get_timestamp_str(ts, sizeof(ts));
    printf("\n[%s] Shutting down...\n", ts);
    
    thread_pool_destroy(g_pool);
    g_pool = NULL;
    
    /* Destroy per-worker connection handlers (workers joined, safe to access) */
    for (unsigned int i = 0; i < pool_config.num_workers && i < MAX_WORKERS; i++) {
        if (g_worker_handlers[i]) {
            connection_handler_destroy(g_worker_handlers[i]);
            g_worker_handlers[i] = NULL;
        }
    }
    
    rate_limiter_destroy(g_shared_rate_limiter);
    g_shared_rate_limiter = NULL;
    
    return 0;
}

static int run_single_threaded(int port, const char *bind_addr, const char *root_dir,
                                unsigned int timeout, bool keep_alive) {
    char ts[32];
    get_timestamp_str(ts, sizeof(ts));
    
    server_config_t config = {
        .port = port,
        .bind_addr = {0},
        .root_dir = {0},
        .max_requests = 100,
        .connection_timeout = timeout,
        .max_request_size = 8192,
        .keep_alive = keep_alive
    };
    strncpy(config.bind_addr, bind_addr, sizeof(config.bind_addr) - 1);
    strncpy(config.root_dir, root_dir, sizeof(config.root_dir) - 1);

    printf("[%s] Server Configuration:\n", ts);
    printf("  Listening: http://%s:%d\n", config.bind_addr, config.port);
    printf("  Root:      %s\n", config.root_dir);
    printf("  Timeout:   %u seconds\n", config.connection_timeout);
    printf("  KeepAlive: %s\n", config.keep_alive ? "enabled" : "disabled");
    printf("\n");

    server_t *server = server_create(&config);
    if (!server) {
        get_timestamp_str(ts, sizeof(ts));
        fprintf(stderr, "[%s] Error: Failed to create server\n", ts);
        return 1;
    }

    if (!server_run(server)) {
        get_timestamp_str(ts, sizeof(ts));
        fprintf(stderr, "[%s] Error: Failed to run server\n", ts);
        server_destroy(server);
        return 1;
    }

    server_destroy(server);
    get_timestamp_str(ts, sizeof(ts));
    printf("[%s] Server stopped\n", ts);
    return 0;
}

int main(int argc, char *argv[]) {
    int port = 8000;
    char bind_addr[64] = "127.0.0.1";
    char root_dir[256] = "www";
    int num_workers = -1;
    unsigned int timeout = 30;
    bool keep_alive = false;
    const char *config_file = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--platform-info") == 0) {
            print_platform_info();
            return 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_file = argv[++i];
        } else if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc) {
            num_workers = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
            strncpy(bind_addr, argv[++i], sizeof(bind_addr) - 1);
        } else if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
            strncpy(root_dir, argv[++i], sizeof(root_dir) - 1);
        } else if (strcmp(argv[i], "--timeout") == 0 && i + 1 < argc) {
            timeout = (unsigned int)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--keep-alive") == 0) {
            keep_alive = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (config_file) {
        int cfg_port = port;
        char cfg_bind[64], cfg_root[256];
        unsigned int cfg_timeout = timeout;
        bool cfg_keepalive = keep_alive;
        strncpy(cfg_bind, bind_addr, sizeof(cfg_bind));
        strncpy(cfg_root, root_dir, sizeof(cfg_root));
        
        if (!load_config_file(config_file, &cfg_port, cfg_bind, sizeof(cfg_bind),
                               cfg_root, sizeof(cfg_root), &cfg_timeout, &cfg_keepalive)) {
            fprintf(stderr, "Error: Could not load config file: %s\n", config_file);
            return 1;
        }
        
        if (port == 8000) port = cfg_port;
        if (strcmp(bind_addr, "127.0.0.1") == 0) strncpy(bind_addr, cfg_bind, sizeof(bind_addr));
        if (strcmp(root_dir, "www") == 0) strncpy(root_dir, cfg_root, sizeof(root_dir));
        if (timeout == 30) timeout = cfg_timeout;
        if (!keep_alive) keep_alive = cfg_keepalive;
    }
    
    char ts[32];
    get_timestamp_str(ts, sizeof(ts));
    printf("\n=== Zircon Web Server ===\n");
    printf("[%s] Starting...\n\n", ts);
    
    if (num_workers >= 0) {
        return run_multi_threaded(port, bind_addr, root_dir,
                                   timeout, keep_alive, (unsigned int)num_workers);
    }
    
    return run_single_threaded(port, bind_addr, root_dir, timeout, keep_alive);
}
