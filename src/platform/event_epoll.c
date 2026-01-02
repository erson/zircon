#include "platform/event_loop.h"

#if defined(ZIRCON_LINUX)

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAX_FD_SLOTS 65536

typedef struct {
    event_callback_t callback;
    void *userdata;
    event_flags_t events;
} fd_data_t;

struct event_loop {
    int epfd;
    struct epoll_event *events;
    int max_events;
    int timeout_ms;
    bool running;
    fd_data_t *fd_data;
};

static uint32_t to_epoll_events(event_flags_t flags) {
    uint32_t ev = 0;
    if (flags & EVENT_READ)  ev |= EPOLLIN;
    if (flags & EVENT_WRITE) ev |= EPOLLOUT;
    if (flags & EVENT_EDGE)  ev |= EPOLLET;
    return ev;
}

static event_flags_t from_epoll_events(uint32_t ev) {
    event_flags_t flags = EVENT_NONE;
    if (ev & EPOLLIN)   flags |= EVENT_READ;
    if (ev & EPOLLOUT)  flags |= EVENT_WRITE;
    if (ev & EPOLLERR)  flags |= EVENT_ERROR;
    if (ev & EPOLLHUP)  flags |= EVENT_HANGUP;
    if (ev & EPOLLRDHUP) flags |= EVENT_HANGUP;
    return flags;
}

event_loop_t *event_loop_create(const event_loop_config_t *config) {
    event_loop_t *loop = calloc(1, sizeof(*loop));
    if (!loop) return NULL;
    
    loop->epfd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epfd < 0) {
        free(loop);
        return NULL;
    }
    
    loop->max_events = config ? config->max_events : 1024;
    loop->timeout_ms = config ? config->timeout_ms : -1;
    
    loop->events = calloc(loop->max_events, sizeof(struct epoll_event));
    if (!loop->events) {
        close(loop->epfd);
        free(loop);
        return NULL;
    }
    
    loop->fd_data = calloc(MAX_FD_SLOTS, sizeof(fd_data_t));
    if (!loop->fd_data) {
        free(loop->events);
        close(loop->epfd);
        free(loop);
        return NULL;
    }
    
    return loop;
}

void event_loop_destroy(event_loop_t *loop) {
    if (!loop) return;
    
    if (loop->epfd >= 0) close(loop->epfd);
    free(loop->events);
    free(loop->fd_data);
    free(loop);
}

int event_loop_add(event_loop_t *loop, socket_t fd, event_flags_t events,
                   event_callback_t callback, void *userdata) {
    if (!loop || fd < 0 || fd >= MAX_FD_SLOTS) return ZIRCON_INVALID;
    
    struct epoll_event ev = {
        .events = to_epoll_events(events),
        .data.fd = fd
    };
    
    if (epoll_ctl(loop->epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        if (errno == EEXIST) {
            if (epoll_ctl(loop->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
                return ZIRCON_ERROR;
            }
        } else {
            return ZIRCON_ERROR;
        }
    }
    
    loop->fd_data[fd].callback = callback;
    loop->fd_data[fd].userdata = userdata;
    loop->fd_data[fd].events = events;
    
    return ZIRCON_OK;
}

int event_loop_modify(event_loop_t *loop, socket_t fd, event_flags_t events) {
    if (!loop || fd < 0 || fd >= MAX_FD_SLOTS) return ZIRCON_INVALID;
    
    struct epoll_event ev = {
        .events = to_epoll_events(events),
        .data.fd = fd
    };
    
    if (epoll_ctl(loop->epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        return ZIRCON_ERROR;
    }
    
    loop->fd_data[fd].events = events;
    return ZIRCON_OK;
}

int event_loop_remove(event_loop_t *loop, socket_t fd) {
    if (!loop || fd < 0 || fd >= MAX_FD_SLOTS) return ZIRCON_INVALID;
    
    epoll_ctl(loop->epfd, EPOLL_CTL_DEL, fd, NULL);
    memset(&loop->fd_data[fd], 0, sizeof(fd_data_t));
    
    return ZIRCON_OK;
}

int event_loop_run_once(event_loop_t *loop, int timeout_ms) {
    if (!loop) return ZIRCON_INVALID;
    
    int n = epoll_wait(loop->epfd, loop->events, loop->max_events, timeout_ms);
    if (n < 0) {
        if (errno == EINTR) return 0;
        return ZIRCON_ERROR;
    }
    
    for (int i = 0; i < n; i++) {
        struct epoll_event *ev = &loop->events[i];
        socket_t fd = ev->data.fd;
        
        if (fd < 0 || fd >= MAX_FD_SLOTS) continue;
        
        fd_data_t *data = &loop->fd_data[fd];
        if (!data->callback) continue;
        
        event_flags_t flags = from_epoll_events(ev->events);
        data->callback(loop, fd, flags, data->userdata);
    }
    
    return n;
}

int event_loop_run(event_loop_t *loop) {
    if (!loop) return ZIRCON_INVALID;
    
    loop->running = true;
    while (loop->running) {
        int result = event_loop_run_once(loop, loop->timeout_ms);
        if (result < 0 && result != ZIRCON_TIMEOUT) {
            return result;
        }
    }
    return ZIRCON_OK;
}

void event_loop_stop(event_loop_t *loop) {
    if (loop) loop->running = false;
}

event_backend_t event_loop_get_backend(event_loop_t *loop) {
    (void)loop;
    return EVENT_BACKEND_EPOLL;
}

const char *event_loop_backend_name(event_backend_t backend) {
    switch (backend) {
        case EVENT_BACKEND_IO_URING: return "io_uring";
        case EVENT_BACKEND_EPOLL:    return "epoll";
        case EVENT_BACKEND_KQUEUE:   return "kqueue";
        case EVENT_BACKEND_IOCP:     return "IOCP";
        case EVENT_BACKEND_POLL:     return "poll";
        default:                     return "unknown";
    }
}

event_backend_t event_loop_best_backend(void) {
    return EVENT_BACKEND_EPOLL;
}

#endif /* ZIRCON_LINUX */
