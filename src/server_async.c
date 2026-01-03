#include "server.h"
#include "http.h"
#include "rate_limiter.h"
#include "security_headers.h"
#include "platform/detect.h"
#include "platform/compat.h"
#include "platform/event_loop.h"
#include "platform/sendfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_CONNECTIONS 10000
#define DEFAULT_READ_BUFFER_SIZE 8192
#define DEFAULT_CONNECTION_TIMEOUT 30

typedef enum {
    CONN_STATE_READING,
    CONN_STATE_PROCESSING,
    CONN_STATE_SENDING_HEADERS,
    CONN_STATE_SENDING_BODY,
    CONN_STATE_DONE
} conn_state_t;

typedef struct connection {
    socket_t fd;
    conn_state_t state;
    char *read_buf;
    size_t read_buf_size;
    size_t read_len;
    char *write_buf;
    size_t write_len;
    size_t write_offset;
    int file_fd;
    off_t file_offset;
    size_t file_size;
    char client_ip[INET_ADDRSTRLEN];
    time_t last_activity;
    bool keep_alive;
    struct server *server;
} connection_t;

struct server {
    socket_t sock_fd;
    server_config_t config;
    volatile bool running;
    rate_limiter_t *rate_limiter;
    event_loop_t *event_loop;
    connection_t *connections[MAX_CONNECTIONS];
    time_t last_timeout_check;
};

static server_t *g_server = NULL;

static void get_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static connection_t *connection_create(socket_t fd, struct server *server) {
    connection_t *conn = calloc(1, sizeof(*conn));
    if (!conn) return NULL;
    
    size_t buf_size = server->config.max_request_size > 0 
                      ? server->config.max_request_size 
                      : DEFAULT_READ_BUFFER_SIZE;
    
    conn->read_buf = malloc(buf_size);
    if (!conn->read_buf) {
        free(conn);
        return NULL;
    }
    
    conn->read_buf_size = buf_size;
    conn->fd = fd;
    conn->state = CONN_STATE_READING;
    conn->file_fd = -1;
    conn->server = server;
    conn->last_activity = time(NULL);
    conn->keep_alive = server->config.keep_alive;
    
    return conn;
}

static void connection_destroy(connection_t *conn) {
    if (!conn) return;
    
    if (conn->file_fd >= 0) close(conn->file_fd);
    free(conn->write_buf);
    free(conn->read_buf);
    free(conn);
}

static void connection_reset(connection_t *conn) {
    conn->state = CONN_STATE_READING;
    conn->read_len = 0;
    free(conn->write_buf);
    conn->write_buf = NULL;
    conn->write_len = 0;
    conn->write_offset = 0;
    if (conn->file_fd >= 0) {
        close(conn->file_fd);
        conn->file_fd = -1;
    }
    conn->file_offset = 0;
    conn->file_size = 0;
    conn->last_activity = time(NULL);
}

static bool has_path_traversal(const char *path, const char *root_dir) {
    if (!path) return true;
    if (strstr(path, "..") || strstr(path, "//") || strstr(path, "\\")) return true;
    if (path[0] != '/') return true;
    
    char root_path[PATH_MAX];
    char requested_path[PATH_MAX];
    char *root_real = realpath(root_dir, root_path);
    if (!root_real) return true;
    
    snprintf(requested_path, sizeof(requested_path), "%s%s", root_dir, path);
    char *req_real = realpath(requested_path, NULL);
    
    if (!req_real) {
        char *last_slash = strrchr(requested_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            req_real = realpath(requested_path, NULL);
            if (!req_real) return true;
            if (strncmp(req_real, root_path, strlen(root_path)) != 0) {
                free(req_real);
                return true;
            }
            free(req_real);
            return false;
        }
        return true;
    }
    
    bool result = (strncmp(req_real, root_path, strlen(root_path)) == 0);
    free(req_real);
    return !result;
}

static bool is_allowed_file_type(const char *path) {
    if (strcmp(path, "/") == 0) return true;
    
    const char *ext = strrchr(path, '.');
    if (!ext) return false;
    
    const char *allowed[] = {
        ".html", ".htm", ".css", ".js", ".txt", ".ico",
        ".png", ".jpg", ".jpeg", ".gif", ".webp", ".svg",
        ".woff", ".woff2", ".ttf", ".eot", ".json", ".xml",
        ".pdf", ".zip", NULL
    };
    
    for (const char **a = allowed; *a; a++) {
        if (strcasecmp(ext, *a) == 0) return true;
    }
    return false;
}

static bool build_file_path(const char *request_path, const char *root_dir,
                            char *filepath, size_t size) {
    if (!request_path || !filepath || size < 5) return false;
    if (has_path_traversal(request_path, root_dir)) return false;
    if (!is_allowed_file_type(request_path)) return false;
    
    strncpy(filepath, root_dir, size);
    filepath[size - 1] = '\0';
    
    if (strcmp(request_path, "/") == 0) {
        strncat(filepath, "/index.html", size - strlen(filepath) - 1);
        return true;
    }
    
    size_t len = strlen(request_path);
    if (len > 0 && request_path[len - 1] == '/') {
        strncat(filepath, request_path, size - strlen(filepath) - 1);
        strncat(filepath, "index.html", size - strlen(filepath) - 1);
        return true;
    }
    
    strncat(filepath, request_path, size - strlen(filepath) - 1);
    return true;
}

static void send_error_response(connection_t *conn, int status, const char *message) {
    char headers[1024];
    const char *connection_header = conn->keep_alive ? "keep-alive" : "close";
    
    int header_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n",
        status, message, strlen(message), connection_header);
    
    add_security_headers(headers + header_len, sizeof(headers) - header_len);
    strncat(headers, "\r\n", sizeof(headers) - strlen(headers) - 1);
    
    size_t total_len = strlen(headers) + strlen(message);
    conn->write_buf = malloc(total_len + 1);
    if (conn->write_buf) {
        strcpy(conn->write_buf, headers);
        strcat(conn->write_buf, message);
        conn->write_len = total_len;
        conn->write_offset = 0;
        conn->state = CONN_STATE_SENDING_HEADERS;
    }
}

static bool check_keep_alive(const char *request) {
    const char *conn_header = strcasestr(request, "Connection:");
    if (!conn_header) return true;
    
    conn_header += 11;
    while (*conn_header == ' ') conn_header++;
    
    if (strncasecmp(conn_header, "close", 5) == 0) return false;
    return true;
}

static void process_request(connection_t *conn) {
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    
    http_request_t req;
    
    if (!http_parse_request(conn->read_buf, conn->read_len, &req)) {
        printf("[%s] Bad request from %s\n", ts, conn->client_ip);
        send_error_response(conn, 400, "Bad Request");
        conn->keep_alive = false;
        return;
    }
    
    if (conn->server->config.keep_alive) {
        conn->keep_alive = check_keep_alive(conn->read_buf);
    }
    
    printf("[%s] %s %s from %s\n", ts,
           req.method == HTTP_GET ? "GET" : req.method == HTTP_HEAD ? "HEAD" : "OTHER",
           req.path, conn->client_ip);
    
    if (req.method != HTTP_GET && req.method != HTTP_HEAD) {
        send_error_response(conn, 405, "Method Not Allowed");
        conn->keep_alive = false;
        return;
    }
    
    if (!is_allowed_file_type(req.path)) {
        send_error_response(conn, 403, "Forbidden");
        return;
    }
    
    const char *root = conn->server->config.root_dir[0] 
                       ? conn->server->config.root_dir 
                       : "www";
    
    char filepath[512];
    if (!build_file_path(req.path, root, filepath, sizeof(filepath))) {
        send_error_response(conn, 403, "Forbidden");
        return;
    }
    
    if (!rate_limiter_check(conn->server->rate_limiter, conn->client_ip)) {
        send_error_response(conn, 429, "Too Many Requests");
        conn->keep_alive = false;
        return;
    }
    
    conn->file_fd = open(filepath, O_RDONLY);
    if (conn->file_fd < 0) {
        send_error_response(conn, 404, "Not Found");
        return;
    }
    
    struct stat st;
    if (fstat(conn->file_fd, &st) < 0) {
        close(conn->file_fd);
        conn->file_fd = -1;
        send_error_response(conn, 500, "Internal Server Error");
        return;
    }
    
    conn->file_size = st.st_size;
    conn->file_offset = 0;
    
    const char *mime_type = http_get_mime_type(filepath);
    char *etag = http_generate_etag(st.st_mtime, st.st_size);
    const char *connection_header = conn->keep_alive ? "keep-alive" : "close";
    
    if (etag && http_check_etag_match(conn->read_buf, etag)) {
        char headers[512];
        int len = snprintf(headers, sizeof(headers),
            "HTTP/1.1 304 Not Modified\r\n"
            "ETag: %s\r\n"
            "Cache-Control: max-age=86400\r\n"
            "Connection: %s\r\n", etag, connection_header);
        add_security_headers(headers + len, sizeof(headers) - len);
        strncat(headers, "\r\n", sizeof(headers) - strlen(headers) - 1);
        
        conn->write_buf = strdup(headers);
        conn->write_len = strlen(headers);
        conn->write_offset = 0;
        conn->state = CONN_STATE_SENDING_HEADERS;
        conn->file_size = 0;
        
        close(conn->file_fd);
        conn->file_fd = -1;
        free(etag);
        
        printf("[%s] %s - 304 Not Modified\n", ts, conn->client_ip);
        return;
    }
    
    char headers[1024];
    int header_len = snprintf(headers, sizeof(headers),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: %s\r\n",
        mime_type, (size_t)st.st_size, connection_header);
    
    if (etag) {
        header_len += snprintf(headers + header_len, sizeof(headers) - header_len,
            "ETag: %s\r\nCache-Control: max-age=86400\r\n", etag);
        free(etag);
    }
    
    add_security_headers(headers + header_len, sizeof(headers) - header_len);
    strncat(headers, "\r\n", sizeof(headers) - strlen(headers) - 1);
    
    conn->write_buf = strdup(headers);
    conn->write_len = strlen(headers);
    conn->write_offset = 0;
    
    if (req.method == HTTP_HEAD) {
        conn->file_size = 0;
        close(conn->file_fd);
        conn->file_fd = -1;
    }
    
    conn->state = CONN_STATE_SENDING_HEADERS;
    printf("[%s] %s - 200 OK - %zu bytes\n", ts, conn->client_ip, (size_t)st.st_size);
}

static void on_connection_event(event_loop_t *loop, socket_t fd, 
                                 event_flags_t events, void *userdata);

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
    
    if (client_fd >= MAX_CONNECTIONS) {
        printf("[%s] Max connections reached\n", ts);
        close(client_fd);
        return;
    }
    
    zircon_set_nonblocking(client_fd);
    
    connection_t *conn = connection_create(client_fd, server);
    if (!conn) {
        close(client_fd);
        return;
    }
    
    inet_ntop(AF_INET, &client_addr.sin_addr, conn->client_ip, sizeof(conn->client_ip));
    server->connections[client_fd] = conn;
    
    printf("[%s] New connection from %s (fd=%d)\n", ts, conn->client_ip, client_fd);
    
    event_loop_add(loop, client_fd, EVENT_READ, on_connection_event, server);
}

static void close_connection(server_t *server, connection_t *conn) {
    char ts[32];
    get_timestamp(ts, sizeof(ts));
    socket_t fd = conn->fd;
    
    event_loop_remove(server->event_loop, fd);
    close(fd);
    
    printf("[%s] Connection closed: %s\n", ts, conn->client_ip);
    
    connection_destroy(conn);
    server->connections[fd] = NULL;
}

static void check_connection_timeouts(server_t *server) {
    if (server->config.connection_timeout == 0) return;
    
    time_t now = time(NULL);
    if (now - server->last_timeout_check < 5) return;
    server->last_timeout_check = now;
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        connection_t *conn = server->connections[i];
        if (!conn) continue;
        
        if (now - conn->last_activity > server->config.connection_timeout) {
            char ts[32];
            get_timestamp(ts, sizeof(ts));
            printf("[%s] Connection timeout: %s\n", ts, conn->client_ip);
            close_connection(server, conn);
        }
    }
}

static void on_connection_event(event_loop_t *loop, socket_t fd,
                                 event_flags_t events, void *userdata) {
    server_t *server = userdata;
    connection_t *conn = server->connections[fd];
    
    if (!conn) return;
    
    conn->last_activity = time(NULL);
    
    if (events & (EVENT_ERROR | EVENT_HANGUP)) {
        close_connection(server, conn);
        return;
    }
    
    if ((events & EVENT_READ) && conn->state == CONN_STATE_READING) {
        size_t remaining = conn->read_buf_size - conn->read_len - 1;
        
        if (remaining == 0) {
            char ts[32];
            get_timestamp(ts, sizeof(ts));
            printf("[%s] Request too large from %s\n", ts, conn->client_ip);
            send_error_response(conn, 413, "Request Entity Too Large");
            conn->keep_alive = false;
            conn->state = CONN_STATE_SENDING_HEADERS;
            event_loop_modify(loop, fd, EVENT_WRITE);
            return;
        }
        
        ssize_t n = read(fd, conn->read_buf + conn->read_len, remaining);
        
        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
            close_connection(server, conn);
            return;
        }
        
        conn->read_len += n;
        conn->read_buf[conn->read_len] = '\0';
        
        if (strstr(conn->read_buf, "\r\n\r\n")) {
            conn->state = CONN_STATE_PROCESSING;
            process_request(conn);
            
            if (conn->state == CONN_STATE_SENDING_HEADERS) {
                event_loop_modify(loop, fd, EVENT_WRITE);
            }
        }
    }
    
    if ((events & EVENT_WRITE) && conn->state == CONN_STATE_SENDING_HEADERS) {
        size_t remaining = conn->write_len - conn->write_offset;
        ssize_t n = write(fd, conn->write_buf + conn->write_offset, remaining);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            close_connection(server, conn);
            return;
        }
        
        conn->write_offset += n;
        
        if (conn->write_offset >= conn->write_len) {
            if (conn->file_fd >= 0 && conn->file_size > 0) {
                conn->state = CONN_STATE_SENDING_BODY;
            } else if (conn->keep_alive) {
                connection_reset(conn);
                event_loop_modify(loop, fd, EVENT_READ);
            } else {
                close_connection(server, conn);
            }
        }
    }
    
    if ((events & EVENT_WRITE) && conn->state == CONN_STATE_SENDING_BODY) {
        size_t remaining = conn->file_size - conn->file_offset;
        size_t to_send = remaining > 65536 ? 65536 : remaining;
        size_t sent = 0;
        
        sendfile_result_t result = platform_sendfile(fd, conn->file_fd, 
                                                      &conn->file_offset, to_send, &sent);
        
        if (result == SENDFILE_ERROR) {
            char buf[8192];
            lseek(conn->file_fd, conn->file_offset, SEEK_SET);
            ssize_t nread = read(conn->file_fd, buf, sizeof(buf) < to_send ? sizeof(buf) : to_send);
            if (nread > 0) {
                ssize_t nwritten = write(fd, buf, nread);
                if (nwritten > 0) {
                    conn->file_offset += nwritten;
                }
            }
        }
        
        if ((size_t)conn->file_offset >= conn->file_size) {
            if (conn->keep_alive) {
                connection_reset(conn);
                event_loop_modify(loop, fd, EVENT_READ);
            } else {
                close_connection(server, conn);
            }
        }
    }
}

static void signal_handler(int sig) {
    (void)sig;
    if (g_server) {
        g_server->running = false;
    }
}

server_t *server_create(const server_config_t *config) {
    if (!config) return NULL;
    
    server_t *server = calloc(1, sizeof(*server));
    if (!server) return NULL;
    
    server->config = *config;
    
    if (server->config.max_request_size == 0) {
        server->config.max_request_size = DEFAULT_READ_BUFFER_SIZE;
    }
    if (server->config.connection_timeout == 0) {
        server->config.connection_timeout = DEFAULT_CONNECTION_TIMEOUT;
    }
    
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
    
    event_loop_config_t ev_config = event_loop_default_config();
    server->event_loop = event_loop_create(&ev_config);
    if (!server->event_loop) {
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }
    
    server->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->sock_fd < 0) {
        event_loop_destroy(server->event_loop);
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
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }
    
    if (listen(server->sock_fd, SOMAXCONN) < 0) {
        close(server->sock_fd);
        event_loop_destroy(server->event_loop);
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }
    
    return server;
}

void server_destroy(server_t *server) {
    if (!server) return;
    
    server->running = false;
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (server->connections[i]) {
            connection_destroy(server->connections[i]);
            server->connections[i] = NULL;
        }
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
        
        check_connection_timeouts(server);
    }
    
    get_timestamp(ts, sizeof(ts));
    printf("\n[%s] Graceful shutdown initiated...\n", ts);
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (server->connections[i]) {
            close_connection(server, server->connections[i]);
        }
    }
    
    printf("[%s] All connections closed\n", ts);
    
    return true;
}
