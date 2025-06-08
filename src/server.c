#include "server.h"
#include "http.h"
#include "rate_limiter.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>

/**
 * Get formatted timestamp for logging purposes
 * 
 * Returns a string representation of the current time in YYYY-MM-DD HH:MM:SS format
 */
static char* get_timestamp(void) {
    static char buffer[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

/* Server context structure */
struct server {
    int sock_fd;
    server_config_t config;
    bool running;
    rate_limiter_t *rate_limiter;
};

/* Check if path contains traversal attempts */
static bool has_path_traversal(const char *path) {
    if (!path) return true;

    /* Check for basic traversal patterns */
    if (strstr(path, "..") || strstr(path, "//") || strstr(path, "\\"))
        return true;

    /* Check for URL-encoded traversal */
    const char *encoded_traversal[] = {
        "%2e%2e", "%2E%2E",  /* .. */
        "%2f",    "%2F",     /* / */
        "%5c",    "%5C",     /* \ */
        NULL
    };

    for (const char **pattern = encoded_traversal; *pattern; pattern++) {
        if (strstr(path, *pattern))
            return true;
    }

    /* Must start with / */
    if (path[0] != '/') {
        return true;
    }

    /* Get absolute paths */
    char www_path[PATH_MAX];
    char requested_path[PATH_MAX];
    char *www_real = realpath("www", www_path);
    if (!www_real) return true;

    /* Build full requested path */
    snprintf(requested_path, sizeof(requested_path), "www%s", path);
    char *req_real = realpath(requested_path, NULL);
    
    /* If path doesn't exist, check if it would be under www */
    if (!req_real) {
        char *last_slash = strrchr(requested_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            req_real = realpath(requested_path, NULL);
            if (!req_real) return true;
            
            /* Check if parent directory is under www */
            if (strncmp(req_real, www_path, strlen(www_path)) != 0) {
                free(req_real);
                return true;
            }
            free(req_real);
            return false;
        }
        return true;
    }

    /* Check if path is under www directory */
    bool result = (strncmp(req_real, www_path, strlen(www_path)) == 0);
    free(req_real);
    return !result;
}

/* Check if file type is allowed */
static bool is_allowed_file_type(const char *path) {
    /* Allow root path */
    if (strcmp(path, "/") == 0) {
        return true;
    }

    /* Get file extension */
    const char *ext = strrchr(path, '.');
    if (!ext) {
        /* No extension - only allow if it's a directory */
        struct stat st;
        char fullpath[PATH_MAX] = "www";
        strncat(fullpath, path, sizeof(fullpath) - 4);
        if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            /* For directories, require trailing slash */
            size_t len = strlen(path);
            return len > 0 && path[len-1] == '/';
        }
        return false;
    }

    /* List of allowed extensions */
    const char *allowed_exts[] = {
        ".html", ".htm",  /* HTML files */
        ".css",          /* Stylesheets */
        ".js",           /* JavaScript */
        ".txt",          /* Text files */
        ".ico",          /* Favicon */
        ".png", ".jpg", ".jpeg", ".gif", ".webp", /* Images */
        ".svg",          /* SVG images */
        ".woff", ".woff2", ".ttf", ".eot",  /* Fonts */
        ".json", ".xml", /* Data files */
        NULL
    };

    /* Make a copy of path for extension checks */
    char *path_copy = strdup(path);
    if (!path_copy) return false;

    /* Check for disallowed extensions anywhere in the path */
    const char *disallowed_exts[] = {
        ".php", ".asp", ".aspx", ".jsp", ".cgi", ".pl", ".py",
        ".sh", ".bash", ".exe", ".dll", ".so",
        NULL
    };

    char *curr_ext = path_copy;
    while ((curr_ext = strchr(curr_ext, '.'))) {
        for (const char **disallowed = disallowed_exts; *disallowed; disallowed++) {
            if (strcasecmp(curr_ext, *disallowed) == 0) {
                free(path_copy);
                return false;
            }
        }
        curr_ext++;
    }
    free(path_copy);

    /* Check against allowed extensions (case-insensitive) */
    for (const char **allowed = allowed_exts; *allowed; allowed++) {
        if (strcasecmp(ext, *allowed) == 0) {
            return true;
        }
    }

    return false;
}

/* Build file path with security checks */
static bool build_file_path(const char *request_path, char *filepath, size_t filepath_size) {
    /* Basic sanity check */
    if (!request_path || !filepath || filepath_size < 5)
        return false;

    /* Check for path traversal */
    if (has_path_traversal(request_path))
        return false;

    /* Check file type */
    if (!is_allowed_file_type(request_path))
        return false;

    /* Initialize with web root */
    strncpy(filepath, "www", filepath_size);
    filepath[filepath_size - 1] = '\0';

    /* Handle root path */
    if (strcmp(request_path, "/") == 0) {
        strncat(filepath, "/index.html", filepath_size - strlen(filepath) - 1);
        return true;
    }

    /* Handle directory index */
    size_t path_len = strlen(request_path);
    if (path_len > 0 && request_path[path_len - 1] == '/') {
        /* Directly construct the path without using a temporary variable */
        if (strlen(filepath) + path_len + 11 < filepath_size) {
            strncat(filepath, request_path, filepath_size - strlen(filepath) - 1);
            strncat(filepath, "index.html", filepath_size - strlen(filepath) - 1);
            return true;
        }
        return false;
    }

    /* Append request path */
    if (strlen(filepath) + strlen(request_path) + 1 > filepath_size)
        return false;  /* Path would be too long */

    strncat(filepath, request_path, filepath_size - strlen(filepath) - 1);
    return true;
}

/* Add security headers to response */
static void add_security_headers(char *headers, size_t size) {
    strncat(headers, 
        "X-Frame-Options: DENY\r\n"
        "X-Content-Type-Options: nosniff\r\n"
        "X-XSS-Protection: 1; mode=block\r\n"
        "Content-Security-Policy: default-src 'self'\r\n"
        "Strict-Transport-Security: max-age=31536000; includeSubDomains\r\n",
        size - strlen(headers) - 1);
}

/* Create server instance */
server_t *server_create(const server_config_t *config) {
    if (!config) return NULL;

    server_t *server = calloc(1, sizeof(*server));
    if (!server) return NULL;

    /* Copy configuration */
    server->config = *config;
    
    /* Initialize rate limiter */
    /* Convert per-minute rate limit to per-second, rounding up to avoid zero */
    unsigned int per_sec = (config->max_requests + 59) / 60;
    if (per_sec == 0) {
        per_sec = 1;
    }

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
    
    /* Create socket */
    server->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->sock_fd < 0) {
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }

    /* Set socket options */
    int opt = 1;
    if (setsockopt(server->sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(server->sock_fd);
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }

    /* Bind socket */
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(config->port),
        .sin_addr.s_addr = inet_addr(config->bind_addr)
    };

    if (bind(server->sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(server->sock_fd);
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }

    /* Start listening */
    if (listen(server->sock_fd, SOMAXCONN) < 0) {
        close(server->sock_fd);
        rate_limiter_destroy(server->rate_limiter);
        free(server);
        return NULL;
    }

    return server;
}

/* Clean up server */
void server_destroy(server_t *server) {
    if (server) {
        server->running = false;
        if (server->sock_fd >= 0) {
            close(server->sock_fd);
        }
        if (server->rate_limiter) {
            rate_limiter_destroy(server->rate_limiter);
        }
        free(server);
    }
}

/* Client connection context */
typedef struct {
    int fd;
    server_t *server;
} client_context_t;

/* Handle client connection */
static void *handle_client(void *arg) {
    client_context_t *ctx = (client_context_t*)arg;
    int client_fd = ctx->fd;
    server_t *server = ctx->server;
    free(ctx);

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    getpeername(client_fd, (struct sockaddr*)&addr, &addr_len);
    
    printf("[%s] New connection from %s\n", get_timestamp(), inet_ntoa(addr.sin_addr));

    char buffer[4096];
    ssize_t bytes = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes <= 0) {
        printf("[%s] Connection closed by %s\n", get_timestamp(), inet_ntoa(addr.sin_addr));
        close(client_fd);
        return NULL;
    }
    buffer[bytes] = '\0';

    /* Parse HTTP request */
    http_request_t req;
    if (!http_parse_request(buffer, bytes, &req)) {
        printf("[%s] Bad request from %s\n", get_timestamp(), inet_ntoa(addr.sin_addr));
        http_send_error(client_fd, 400, "Bad Request");
        close(client_fd);
        return NULL;
    }

    printf("[%s] Request: %s %s from %s\n", 
           get_timestamp(),
           req.method == HTTP_GET ? "GET" :
           req.method == HTTP_POST ? "POST" :
           req.method == HTTP_HEAD ? "HEAD" : "UNKNOWN",
           req.path,
           inet_ntoa(addr.sin_addr));

    /* Validate file type */
    if (!is_allowed_file_type(req.path)) {
        printf("[%s] Forbidden request for %s from %s\n", 
               get_timestamp(), req.path, inet_ntoa(addr.sin_addr));
        http_send_error(client_fd, 403, "Forbidden");
        close(client_fd);
        return NULL;
    }

    /* Build file path with security checks */
    char filepath[512];
    if (!build_file_path(req.path, filepath, sizeof(filepath))) {
        printf("[%s] Invalid path: %s from %s\n", 
               get_timestamp(), req.path, inet_ntoa(addr.sin_addr));
        http_send_error(client_fd, 403, "Forbidden");
        close(client_fd);
        return NULL;
    }

    /* Check rate limit after security checks */
    if (!rate_limiter_check(server->rate_limiter, inet_ntoa(addr.sin_addr))) {
        printf("[%s] Rate limit exceeded for %s\n", get_timestamp(), inet_ntoa(addr.sin_addr));
        http_send_error(client_fd, 429, "Too Many Requests");
        close(client_fd);
        return NULL;
    }

    /* Open and send file */
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        printf("[%s] File not found: %s (requested by %s)\n", 
               get_timestamp(), filepath, inet_ntoa(addr.sin_addr));
        http_send_error(client_fd, 404, "Not Found");
        close(client_fd);
        return NULL;
    }

    /* Get file stats (size, modification time) */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        printf("[%s] Error reading file: %s\n", get_timestamp(), filepath);
        close(fd);
        http_send_error(client_fd, 500, "Internal Server Error");
        close(client_fd);
        return NULL;
    }
    
    /* Generate ETag based on file metadata */
    char *etag = http_generate_etag(st.st_mtime, st.st_size);
    if (etag) {
        /* Check if client already has this version */
        if (http_check_etag_match(buffer, etag)) {
            /* Send 304 Not Modified */
            char headers[512] = {0};
            snprintf(headers, sizeof(headers), "ETag: %s\r\nCache-Control: max-age=86400\r\n", etag);
            add_security_headers(headers, sizeof(headers));
            
            http_send_response(client_fd, 304, "", NULL, 0, headers);
            
            printf("[%s] %s - %s %s - 304 Not Modified\n", 
                   get_timestamp(), inet_ntoa(addr.sin_addr),
                   req.method == HTTP_GET ? "GET" : "HEAD", 
                   req.path);
            
            free(etag);
            close(fd);
            close(client_fd);
            return NULL;
        }
    }

    /* Handle HEAD request (no body) */
    if (req.method == HTTP_HEAD) {
        /* Prepare response headers */
        char headers[4096] = {0};
        const char *mime_type = http_get_mime_type(filepath);
        
        /* Add cache headers */
        if (etag) {
            snprintf(headers, sizeof(headers), "ETag: %s\r\nCache-Control: max-age=86400\r\n", etag);
            free(etag);
        } else {
            strncpy(headers, "Cache-Control: max-age=3600\r\n", sizeof(headers) - 1);
        }
        
        /* Add security headers */
        add_security_headers(headers, sizeof(headers));
        
        /* Send HEAD response */
        http_send_response(client_fd, 200, mime_type, NULL, st.st_size, headers);
        
        printf("[%s] %s - HEAD %s - 200 OK - %ld bytes\n", 
               get_timestamp(), inet_ntoa(addr.sin_addr), req.path, (long)st.st_size);
        
        close(fd);
        close(client_fd);
        return NULL;
    }
    
    /* For GET requests, read and send file */
    char *content = malloc(st.st_size);
    if (!content) {
        printf("[%s] Memory allocation failed for file: %s\n", get_timestamp(), filepath);
        close(fd);
        http_send_error(client_fd, 500, "Internal Server Error");
        close(client_fd);
        return NULL;
    }

    if (read(fd, content, st.st_size) != st.st_size) {
        printf("[%s] Error reading file: %s\n", get_timestamp(), filepath);
        free(content);
        close(fd);
        http_send_error(client_fd, 500, "Internal Server Error");
        close(client_fd);
        return NULL;
    }

        /* Prepare response headers with ETag and caching information */
    char headers[4096] = {0};
    const char *mime_type = http_get_mime_type(filepath);
    
    /* Add cache headers with ETag for optimal caching */
    if (etag) {
        /* 24-hour cache for resources with ETag */
        snprintf(headers, sizeof(headers), "ETag: %s\r\nCache-Control: max-age=86400\r\n", etag);
        free(etag);
    } else {
        /* 1-hour cache for resources without ETag */
        strncpy(headers, "Cache-Control: max-age=3600\r\n", sizeof(headers) - 1);
    }
    
    /* Add security headers for better web protection */
    add_security_headers(headers, sizeof(headers));
    
    /* Send response */
    http_send_response(client_fd, 200, mime_type, content, st.st_size, headers);
    
    /* Log access */
    printf("[%s] %s - %s %s - 200 OK - %ld bytes - %s\n", 
           get_timestamp(), inet_ntoa(addr.sin_addr), 
           req.method == HTTP_GET ? "GET" : req.method == HTTP_HEAD ? "HEAD" : "POST",
           req.path, (long)st.st_size, mime_type);

    /* Clean up */
    free(content);
    close(fd);
    close(client_fd);
    printf("[%s] Connection closed: %s\n", get_timestamp(), inet_ntoa(addr.sin_addr));
    return NULL;
}

/* Run server */
bool server_run(server_t *server) {
    if (!server || server->sock_fd < 0) return false;

    server->running = true;
    printf("[%s] Server is running and ready for connections\n", get_timestamp());
    
    while (server->running) {
        /* Accept client connection */
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        client_context_t *ctx = malloc(sizeof(client_context_t));
        if (!ctx) {
            printf("[%s] Memory allocation failed for new connection\n", get_timestamp());
            continue;
        }

        ctx->fd = accept(server->sock_fd, (struct sockaddr*)&client_addr, &client_len);
        if (ctx->fd < 0) {
            printf("[%s] Failed to accept connection\n", get_timestamp());
            free(ctx);
            continue;
        }
        ctx->server = server;

        /* Create thread to handle client */
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, ctx) != 0) {
            printf("[%s] Failed to create thread for client: %s\n", 
                   get_timestamp(), inet_ntoa(client_addr.sin_addr));
            close(ctx->fd);
            free(ctx);
            continue;
        }
        pthread_detach(thread);
    }

    return true;
}
