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
} server_config_t;

/* Function prototypes */
server_t *server_create(const server_config_t *config);
void server_destroy(server_t *server);
bool server_run(server_t *server);

#endif /* SERVER_H */
