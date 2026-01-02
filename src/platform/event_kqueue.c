#include "platform/event_loop.h"

#if defined(ZIRCON_HAS_KQUEUE)

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/event.h>

#define MAX_FD_SLOTS 65536

typedef struct {
    event_callback_t callback;
    void *userdata;
    event_flags_t events;
} fd_data_t;

struct event_loop {
    int kq;
    struct kevent *events;
    int max_events;
    int timeout_ms;
    bool running;
    fd_data_t *fd_data;
};

event_loop_t *event_loop_create(const event_loop_config_t *config) {
    event_loop_t *loop = calloc(1, sizeof(*loop));
    if (!loop) return NULL;
    
    loop->kq = kqueue();
    if (loop->kq < 0) {
        free(loop);
        return NULL;
    }
    
    loop->max_events = config ? config->max_events : 1024;
    loop->timeout_ms = config ? config->timeout_ms : -1;
    
    loop->events = calloc(loop->max_events, sizeof(struct kevent));
    if (!loop->events) {
        close(loop->kq);
        free(loop);
        return NULL;
    }
    
    loop->fd_data = calloc(MAX_FD_SLOTS, sizeof(fd_data_t));
    if (!loop->fd_data) {
        free(loop->events);
        close(loop->kq);
        free(loop);
        return NULL;
    }
    
    return loop;
}

void event_loop_destroy(event_loop_t *loop) {
    if (!loop) return;
    
    if (loop->kq >= 0) close(loop->kq);
    free(loop->events);
    free(loop->fd_data);
    free(loop);
}

int event_loop_add(event_loop_t *loop, socket_t fd, event_flags_t events,
                   event_callback_t callback, void *userdata) {
    if (!loop || fd < 0 || fd >= MAX_FD_SLOTS) return ZIRCON_INVALID;
    
    struct kevent changes[2];
    int nchanges = 0;
    
    if (events & EVENT_READ) {
        EV_SET(&changes[nchanges++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }
    if (events & EVENT_WRITE) {
        EV_SET(&changes[nchanges++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }
    
    if (kevent(loop->kq, changes, nchanges, NULL, 0, NULL) < 0) {
        return ZIRCON_ERROR;
    }
    
    loop->fd_data[fd].callback = callback;
    loop->fd_data[fd].userdata = userdata;
    loop->fd_data[fd].events = events;
    
    return ZIRCON_OK;
}

int event_loop_modify(event_loop_t *loop, socket_t fd, event_flags_t events) {
    if (!loop || fd < 0 || fd >= MAX_FD_SLOTS) return ZIRCON_INVALID;
    
    fd_data_t *data = &loop->fd_data[fd];
    event_flags_t old_events = data->events;
    
    struct kevent changes[4];
    int nchanges = 0;
    
    if ((old_events & EVENT_READ) && !(events & EVENT_READ)) {
        EV_SET(&changes[nchanges++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    } else if (!(old_events & EVENT_READ) && (events & EVENT_READ)) {
        EV_SET(&changes[nchanges++], fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }
    
    if ((old_events & EVENT_WRITE) && !(events & EVENT_WRITE)) {
        EV_SET(&changes[nchanges++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    } else if (!(old_events & EVENT_WRITE) && (events & EVENT_WRITE)) {
        EV_SET(&changes[nchanges++], fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, NULL);
    }
    
    if (nchanges > 0 && kevent(loop->kq, changes, nchanges, NULL, 0, NULL) < 0) {
        return ZIRCON_ERROR;
    }
    
    data->events = events;
    return ZIRCON_OK;
}

int event_loop_remove(event_loop_t *loop, socket_t fd) {
    if (!loop || fd < 0 || fd >= MAX_FD_SLOTS) return ZIRCON_INVALID;
    
    fd_data_t *data = &loop->fd_data[fd];
    struct kevent changes[2];
    int nchanges = 0;
    
    if (data->events & EVENT_READ) {
        EV_SET(&changes[nchanges++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    }
    if (data->events & EVENT_WRITE) {
        EV_SET(&changes[nchanges++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    }
    
    kevent(loop->kq, changes, nchanges, NULL, 0, NULL);
    
    memset(data, 0, sizeof(*data));
    return ZIRCON_OK;
}

int event_loop_run_once(event_loop_t *loop, int timeout_ms) {
    if (!loop) return ZIRCON_INVALID;
    
    struct timespec ts;
    struct timespec *pts = NULL;
    
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000;
        pts = &ts;
    }
    
    int n = kevent(loop->kq, NULL, 0, loop->events, loop->max_events, pts);
    if (n < 0) return ZIRCON_ERROR;
    
    for (int i = 0; i < n; i++) {
        struct kevent *ev = &loop->events[i];
        socket_t fd = (socket_t)ev->ident;
        
        if (fd < 0 || fd >= MAX_FD_SLOTS) continue;
        
        fd_data_t *data = &loop->fd_data[fd];
        if (!data->callback) continue;
        
        event_flags_t flags = EVENT_NONE;
        if (ev->filter == EVFILT_READ) flags |= EVENT_READ;
        if (ev->filter == EVFILT_WRITE) flags |= EVENT_WRITE;
        if (ev->flags & EV_EOF) flags |= EVENT_HANGUP;
        if (ev->flags & EV_ERROR) flags |= EVENT_ERROR;
        
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
    return EVENT_BACKEND_KQUEUE;
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
    return EVENT_BACKEND_KQUEUE;
}

#endif /* ZIRCON_HAS_KQUEUE */
