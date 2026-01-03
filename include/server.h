#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include <stdbool.h>

/* Server context */
typedef struct server server_t;

/* Server configuration */
typedef struct {
    uint16_t port;              /* Server port */
    char bind_addr[16];         /* Bind address */
    char root_dir[256];         /* Web root directory */
    uint32_t max_requests;      /* Rate limit: max requests per minute */
    uint32_t connection_timeout;/* Idle connection timeout in seconds (0=disabled) */
    uint32_t max_request_size;  /* Max request size in bytes (default 8KB) */
    bool keep_alive;            /* Enable HTTP Keep-Alive */
} server_config_t;

/* Function prototypes */
server_t *server_create(const server_config_t *config);
void server_destroy(server_t *server);
bool server_run(server_t *server);
void server_stop(server_t *server);

#endif /* SERVER_H */
