#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "server.h"
#include "logger.h"
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

static char* get_timestamp(void) {
    static char buffer[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
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
    printf("  --platform-info    Show platform capabilities\n");
    printf("  --workers N        Run with N worker threads (0=auto, default=single-threaded)\n");
    printf("  --port PORT        Listen on PORT (default: 8000)\n");
    printf("  --bind ADDR        Bind to ADDR (default: 127.0.0.1)\n");
    printf("  --help             Show this help\n");
}

static void signal_handler(int sig) {
    (void)sig;
    g_shutdown = 1;
    if (g_pool) {
        thread_pool_stop(g_pool);
    }
}

static void on_worker_accept(worker_t *worker, socket_t client_fd,
                              struct sockaddr_in *client_addr, void *userdata) {
    (void)userdata;
    
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, client_ip, sizeof(client_ip));
    
    char response[] = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 13\r\n"
        "Connection: close\r\n"
        "\r\n"
        "Hello World!\n";
    
    write(client_fd, response, sizeof(response) - 1);
    close(client_fd);
    
    worker_stat_connection_closed(worker);
    worker_stat_request_processed(worker);
}

static int run_multi_threaded(int port, const char *bind_addr, unsigned int num_workers) {
    printf("[%s] Multi-threaded mode: %u workers\n", get_timestamp(), 
           num_workers == 0 ? platform_cpu_count() : num_workers);
    
    thread_pool_config_t pool_config = thread_pool_default_config();
    pool_config.num_workers = num_workers;
    pool_config.pin_to_cpu = true;
    pool_config.worker.port = port;
    pool_config.worker.bind_addr = bind_addr;
    pool_config.worker.backlog = 1024;
    pool_config.worker.max_connections = 10000;
    pool_config.worker.on_accept = on_worker_accept;
    pool_config.worker.userdata = NULL;
    
    g_pool = thread_pool_create(&pool_config);
    if (!g_pool) {
        fprintf(stderr, "[%s] Error: Failed to create thread pool\n", get_timestamp());
        return 1;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (thread_pool_start(g_pool) < 0) {
        fprintf(stderr, "[%s] Error: Failed to start thread pool\n", get_timestamp());
        thread_pool_destroy(g_pool);
        return 1;
    }
    
    printf("[%s] Server running on http://%s:%d (multi-threaded)\n",
           get_timestamp(), bind_addr, port);
    
    while (!g_shutdown) {
        sleep(1);
        
        worker_stats_t stats;
        thread_pool_get_stats(g_pool, &stats);
        
        static uint64_t last_requests = 0;
        uint64_t rps = stats.requests_processed - last_requests;
        last_requests = stats.requests_processed;
        
        if (rps > 0) {
            printf("[%s] Stats: %llu active, %llu total, %llu req/s\n",
                   get_timestamp(),
                   (unsigned long long)stats.connections_active,
                   (unsigned long long)stats.connections_accepted,
                   (unsigned long long)rps);
        }
    }
    
    printf("\n[%s] Shutting down...\n", get_timestamp());
    thread_pool_destroy(g_pool);
    g_pool = NULL;
    
    return 0;
}

static int run_single_threaded(int port, const char *bind_addr) {
    server_config_t config = {
        .port = port,
        .bind_addr = {0},
        .root_dir = "www",
        .max_requests = 100
    };
    strncpy(config.bind_addr, bind_addr, sizeof(config.bind_addr) - 1);

    printf("[%s] Server Configuration:\n", get_timestamp());
    printf("- Listening on: http://%s:%d\n", config.bind_addr, config.port);
    printf("- Web root: %s\n", config.root_dir);
    printf("- Rate limit: %d requests/minute\n", config.max_requests);
    printf("\n[%s] Initializing server...\n", get_timestamp());

    server_t *server = server_create(&config);
    if (!server) {
        fprintf(stderr, "[%s] Error: Failed to create server\n", get_timestamp());
        return 1;
    }
    printf("[%s] Server created successfully\n", get_timestamp());

    printf("\n[%s] Starting server...\n", get_timestamp());
    if (!server_run(server)) {
        fprintf(stderr, "[%s] Error: Failed to run server\n", get_timestamp());
        server_destroy(server);
        return 1;
    }

    printf("\n[%s] Server shutting down...\n", get_timestamp());
    server_destroy(server);
    printf("[%s] Server stopped\n", get_timestamp());
    return 0;
}

int main(int argc, char *argv[]) {
    int port = 8000;
    const char *bind_addr = "127.0.0.1";
    int num_workers = -1;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--platform-info") == 0) {
            print_platform_info();
            return 0;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc) {
            num_workers = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
            bind_addr = argv[++i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    printf("\n=== Zircon Secure Web Server ===\n");
    printf("[%s] Server starting up\n\n", get_timestamp());
    
    if (num_workers >= 0) {
        return run_multi_threaded(port, bind_addr, (unsigned int)num_workers);
    }
    
    return run_single_threaded(port, bind_addr);
}
