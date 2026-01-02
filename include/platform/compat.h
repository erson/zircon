#ifndef PLATFORM_COMPAT_H
#define PLATFORM_COMPAT_H

#include "detect.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#if defined(ZIRCON_UNIX)
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <pthread.h>
    
    typedef int socket_t;
    typedef pthread_t zthread_t;
    typedef pthread_mutex_t mutex_t;
    
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
    #define closesocket close
    
#elif defined(ZIRCON_WINDOWS)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    
    typedef SOCKET socket_t;
    typedef HANDLE zthread_t;
    typedef CRITICAL_SECTION mutex_t;
    
    typedef int socklen_t;
    typedef int ssize_t;
    
    #pragma comment(lib, "ws2_32.lib")
#endif

#if defined(ZIRCON_LINUX)
    #include <sys/epoll.h>
    #include <sys/sendfile.h>
    #include <sys/eventfd.h>
#endif

#if defined(ZIRCON_HAS_KQUEUE)
    #include <sys/event.h>
#endif

typedef enum {
    ZIRCON_OK = 0,
    ZIRCON_ERROR = -1,
    ZIRCON_AGAIN = -2,
    ZIRCON_NOMEM = -3,
    ZIRCON_INVALID = -4,
    ZIRCON_TIMEOUT = -5,
    ZIRCON_CLOSED = -6,
    ZIRCON_BUSY = -7,
    ZIRCON_UNSUPPORTED = -8
} zircon_status_t;

ZIRCON_ALWAYS_INLINE static int zircon_get_last_error(void) {
#if defined(ZIRCON_WINDOWS)
    return WSAGetLastError();
#else
    return errno;
#endif
}

ZIRCON_ALWAYS_INLINE static bool zircon_would_block(void) {
#if defined(ZIRCON_WINDOWS)
    int err = WSAGetLastError();
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
#endif
}

ZIRCON_ALWAYS_INLINE static int zircon_set_nonblocking(socket_t fd) {
#if defined(ZIRCON_UNIX)
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#elif defined(ZIRCON_WINDOWS)
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode);
#endif
}

ZIRCON_ALWAYS_INLINE static int zircon_set_reuseaddr(socket_t fd) {
    int opt = 1;
#if defined(ZIRCON_WINDOWS)
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
}

#if defined(ZIRCON_HAS_SO_REUSEPORT)
ZIRCON_ALWAYS_INLINE static int zircon_set_reuseport(socket_t fd) {
    int opt = 1;
    return setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
}
#else
ZIRCON_ALWAYS_INLINE static int zircon_set_reuseport(socket_t fd) {
    (void)fd;
    return ZIRCON_UNSUPPORTED;
}
#endif

#if defined(ZIRCON_WINDOWS)
static inline int platform_socket_init(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa);
}
static inline void platform_socket_cleanup(void) {
    WSACleanup();
}
#else
static inline int platform_socket_init(void) { return 0; }
static inline void platform_socket_cleanup(void) {}
#endif

#endif /* PLATFORM_COMPAT_H */
