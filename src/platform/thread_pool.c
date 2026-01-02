#include "platform/thread_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

unsigned int platform_cpu_count(void);
int platform_thread_set_affinity(pthread_t thread, unsigned int cpu);
int platform_thread_set_name(const char *name);

struct worker {
    unsigned int id;
    pthread_t thread;
    event_loop_t *event_loop;
    socket_t listen_fd;
    thread_pool_t *pool;
    
    volatile bool running;
    
    worker_stats_t stats;
    
    worker_accept_cb on_accept;
    void *userdata;
    int max_connections;
};

struct thread_pool {
    worker_t *workers;
    unsigned int num_workers;
    bool pin_to_cpu;
    volatile bool running;
    
    worker_config_t config;
};

static socket_t create_listen_socket(const char *bind_addr, uint16_t port, int backlog) {
    socket_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    
    zircon_set_reuseaddr(fd);
    
#if defined(ZIRCON_HAS_SO_REUSEPORT)
    if (zircon_set_reuseport(fd) < 0) {
        perror("SO_REUSEPORT");
    }
#endif
    
    zircon_set_nonblocking(fd);
    
    int opt = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (bind_addr && strcmp(bind_addr, "0.0.0.0") != 0) {
        if (inet_pton(AF_INET, bind_addr, &addr.sin_addr) != 1) {
            close(fd);
            return -1;
        }
    } else {
        addr.sin_addr.s_addr = INADDR_ANY;
    }
    
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }
    
    if (listen(fd, backlog) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }
    
    return fd;
}

static void on_accept_event(event_loop_t *loop, socket_t fd, 
                            event_flags_t events, void *userdata) {
    (void)loop;
    worker_t *worker = (worker_t *)userdata;
    
    if (events & (EVENT_ERROR | EVENT_HANGUP)) {
        return;
    }
    
    if (!(events & EVENT_READ)) {
        return;
    }
    
    for (int i = 0; i < 16; i++) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        
        socket_t client_fd = accept(fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (zircon_would_block()) {
                break;
            }
            continue;
        }
        
        zircon_set_nonblocking(client_fd);
        
        int opt = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        
        worker->stats.connections_accepted++;
        worker->stats.connections_active++;
        
        if (worker->on_accept) {
            worker->on_accept(worker, client_fd, &client_addr, worker->userdata);
        } else {
            close(client_fd);
            worker->stats.connections_active--;
        }
    }
}

static void *worker_thread_main(void *arg) {
    worker_t *worker = (worker_t *)arg;
    thread_pool_t *pool = worker->pool;
    
    char name[16];
    snprintf(name, sizeof(name), "zircon-w%u", worker->id);
    platform_thread_set_name(name);
    
    if (pool->pin_to_cpu) {
        platform_thread_set_affinity(pthread_self(), worker->id);
    }
    
    worker->listen_fd = create_listen_socket(
        pool->config.bind_addr,
        pool->config.port,
        pool->config.backlog
    );
    
    if (worker->listen_fd < 0) {
        fprintf(stderr, "Worker %u: failed to create listen socket\n", worker->id);
        return NULL;
    }
    
    event_loop_config_t loop_config = event_loop_default_config();
    loop_config.max_events = worker->max_connections;
    
    worker->event_loop = event_loop_create(&loop_config);
    if (!worker->event_loop) {
        fprintf(stderr, "Worker %u: failed to create event loop\n", worker->id);
        close(worker->listen_fd);
        return NULL;
    }
    
    if (event_loop_add(worker->event_loop, worker->listen_fd, 
                       EVENT_READ, on_accept_event, worker) != ZIRCON_OK) {
        fprintf(stderr, "Worker %u: failed to add listen socket to event loop\n", worker->id);
        event_loop_destroy(worker->event_loop);
        close(worker->listen_fd);
        return NULL;
    }
    
    printf("Worker %u: listening on port %d (CPU %u)\n", 
           worker->id, pool->config.port, worker->id);
    
    worker->running = true;
    
    while (worker->running && pool->running) {
        int result = event_loop_run_once(worker->event_loop, 100);
        if (result < 0 && result != ZIRCON_TIMEOUT) {
            break;
        }
    }
    
    event_loop_destroy(worker->event_loop);
    close(worker->listen_fd);
    worker->event_loop = NULL;
    worker->listen_fd = -1;
    
    printf("Worker %u: shutdown complete\n", worker->id);
    return NULL;
}

event_loop_t *worker_get_event_loop(worker_t *worker) {
    return worker ? worker->event_loop : NULL;
}

unsigned int worker_get_id(worker_t *worker) {
    return worker ? worker->id : 0;
}

void worker_get_stats(worker_t *worker, worker_stats_t *stats) {
    if (worker && stats) {
        *stats = worker->stats;
    }
}

void worker_stat_connection_accepted(worker_t *worker) {
    if (worker) worker->stats.connections_accepted++;
}

void worker_stat_connection_closed(worker_t *worker) {
    if (worker && worker->stats.connections_active > 0) {
        worker->stats.connections_active--;
    }
}

void worker_stat_bytes_received(worker_t *worker, size_t bytes) {
    if (worker) worker->stats.bytes_received += bytes;
}

void worker_stat_bytes_sent(worker_t *worker, size_t bytes) {
    if (worker) worker->stats.bytes_sent += bytes;
}

void worker_stat_request_processed(worker_t *worker) {
    if (worker) worker->stats.requests_processed++;
}

thread_pool_t *thread_pool_create(const thread_pool_config_t *config) {
    if (!config) return NULL;
    
    thread_pool_t *pool = calloc(1, sizeof(*pool));
    if (!pool) return NULL;
    
    pool->num_workers = config->num_workers;
    if (pool->num_workers == 0) {
        pool->num_workers = platform_cpu_count();
    }
    
    if (pool->num_workers < 1) pool->num_workers = 1;
    if (pool->num_workers > 64) pool->num_workers = 64;
    
    pool->pin_to_cpu = config->pin_to_cpu;
    pool->config = config->worker;
    pool->running = false;
    
    pool->workers = calloc(pool->num_workers, sizeof(worker_t));
    if (!pool->workers) {
        free(pool);
        return NULL;
    }
    
    for (unsigned int i = 0; i < pool->num_workers; i++) {
        worker_t *w = &pool->workers[i];
        w->id = i;
        w->pool = pool;
        w->listen_fd = -1;
        w->event_loop = NULL;
        w->running = false;
        w->on_accept = config->worker.on_accept;
        w->userdata = config->worker.userdata;
        w->max_connections = config->worker.max_connections;
        memset(&w->stats, 0, sizeof(w->stats));
    }
    
    return pool;
}

int thread_pool_start(thread_pool_t *pool) {
    if (!pool || pool->running) return -1;
    
    pool->running = true;
    
    for (unsigned int i = 0; i < pool->num_workers; i++) {
        worker_t *w = &pool->workers[i];
        if (pthread_create(&w->thread, NULL, worker_thread_main, w) != 0) {
            fprintf(stderr, "Failed to create worker thread %u\n", i);
            pool->running = false;
            return -1;
        }
    }
    
    return 0;
}

void thread_pool_stop(thread_pool_t *pool) {
    if (!pool || !pool->running) return;
    
    pool->running = false;
    
    for (unsigned int i = 0; i < pool->num_workers; i++) {
        worker_t *w = &pool->workers[i];
        w->running = false;
        if (w->event_loop) {
            event_loop_stop(w->event_loop);
        }
    }
    
    for (unsigned int i = 0; i < pool->num_workers; i++) {
        worker_t *w = &pool->workers[i];
        pthread_join(w->thread, NULL);
    }
}

void thread_pool_destroy(thread_pool_t *pool) {
    if (!pool) return;
    
    if (pool->running) {
        thread_pool_stop(pool);
    }
    
    free(pool->workers);
    free(pool);
}

unsigned int thread_pool_num_workers(thread_pool_t *pool) {
    return pool ? pool->num_workers : 0;
}

void thread_pool_get_stats(thread_pool_t *pool, worker_stats_t *stats) {
    if (!pool || !stats) return;
    
    memset(stats, 0, sizeof(*stats));
    
    for (unsigned int i = 0; i < pool->num_workers; i++) {
        worker_stats_t *ws = &pool->workers[i].stats;
        stats->connections_accepted += ws->connections_accepted;
        stats->connections_active += ws->connections_active;
        stats->bytes_received += ws->bytes_received;
        stats->bytes_sent += ws->bytes_sent;
        stats->requests_processed += ws->requests_processed;
    }
}

bool thread_pool_is_running(thread_pool_t *pool) {
    return pool ? pool->running : false;
}
