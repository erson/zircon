#ifndef PLATFORM_EVENT_LOOP_H
#define PLATFORM_EVENT_LOOP_H

#include "platform/detect.h"
#include "platform/compat.h"

typedef enum {
    EVENT_NONE   = 0,
    EVENT_READ   = 1 << 0,
    EVENT_WRITE  = 1 << 1,
    EVENT_ERROR  = 1 << 2,
    EVENT_HANGUP = 1 << 3,
    EVENT_EDGE   = 1 << 4
} event_flags_t;

typedef enum {
    EVENT_BACKEND_AUTO,
    EVENT_BACKEND_IO_URING,
    EVENT_BACKEND_EPOLL,
    EVENT_BACKEND_KQUEUE,
    EVENT_BACKEND_IOCP,
    EVENT_BACKEND_POLL
} event_backend_t;

typedef struct event_loop event_loop_t;

typedef void (*event_callback_t)(event_loop_t *loop, socket_t fd, 
                                  event_flags_t events, void *userdata);

typedef struct {
    event_backend_t preferred_backend;
    int max_events;
    int timeout_ms;
} event_loop_config_t;

ZIRCON_API event_loop_t *event_loop_create(const event_loop_config_t *config);
ZIRCON_API void event_loop_destroy(event_loop_t *loop);

ZIRCON_API int event_loop_add(event_loop_t *loop, socket_t fd, 
                               event_flags_t events,
                               event_callback_t callback, void *userdata);

ZIRCON_API int event_loop_modify(event_loop_t *loop, socket_t fd,
                                  event_flags_t events);

ZIRCON_API int event_loop_remove(event_loop_t *loop, socket_t fd);

ZIRCON_API int event_loop_run_once(event_loop_t *loop, int timeout_ms);

ZIRCON_API int event_loop_run(event_loop_t *loop);

ZIRCON_API void event_loop_stop(event_loop_t *loop);

ZIRCON_API event_backend_t event_loop_get_backend(event_loop_t *loop);

ZIRCON_API const char *event_loop_backend_name(event_backend_t backend);

ZIRCON_API event_backend_t event_loop_best_backend(void);

static inline event_loop_config_t event_loop_default_config(void) {
    event_loop_config_t config = {
        .preferred_backend = EVENT_BACKEND_AUTO,
        .max_events = 1024,
        .timeout_ms = -1
    };
    return config;
}

#endif /* PLATFORM_EVENT_LOOP_H */
