#include "platform/sendfile.h"
#include <errno.h>

#if defined(ZIRCON_LINUX)
#include <sys/sendfile.h>
#endif

#if defined(ZIRCON_MACOS) || defined(ZIRCON_FREEBSD)
#include <sys/socket.h>
#include <sys/uio.h>
#endif

#if defined(ZIRCON_WINDOWS)
#include <mswsock.h>
#endif

__attribute__((unused))
static sendfile_result_t sendfile_userspace_fallback(socket_t out_fd, int in_fd,
                                                      off_t *offset, size_t count,
                                                      size_t *bytes_sent) {
    char buf[8192];
    size_t total = 0;
    
    if (offset && lseek(in_fd, *offset, SEEK_SET) < 0) {
        return SENDFILE_ERROR;
    }
    
    while (total < count) {
        size_t to_read = count - total;
        if (to_read > sizeof(buf)) to_read = sizeof(buf);
        
        ssize_t nread = read(in_fd, buf, to_read);
        if (nread < 0) {
            if (errno == EINTR) continue;
            *bytes_sent = total;
            return SENDFILE_ERROR;
        }
        if (nread == 0) break;
        
        ssize_t nwritten = write(out_fd, buf, nread);
        if (nwritten < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                *bytes_sent = total;
                return SENDFILE_AGAIN;
            }
            *bytes_sent = total;
            return SENDFILE_ERROR;
        }
        
        total += nwritten;
        if (offset) *offset += nwritten;
    }
    
    *bytes_sent = total;
    return SENDFILE_OK;
}

#if defined(ZIRCON_LINUX)

sendfile_result_t platform_sendfile(socket_t out_fd, int in_fd,
                                     off_t *offset, size_t count,
                                     size_t *bytes_sent) {
    ssize_t ret = sendfile(out_fd, in_fd, offset, count);
    
    if (ret >= 0) {
        *bytes_sent = (size_t)ret;
        return SENDFILE_OK;
    }
    
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        *bytes_sent = 0;
        return SENDFILE_AGAIN;
    }
    
    if (errno == EINVAL || errno == ENOSYS) {
        return sendfile_userspace_fallback(out_fd, in_fd, offset, count, bytes_sent);
    }
    
    *bytes_sent = 0;
    return SENDFILE_ERROR;
}

bool platform_has_sendfile_support(void) {
    return true;
}

const char *platform_sendfile_method(void) {
    return "sendfile (Linux)";
}

#elif defined(ZIRCON_MACOS)

sendfile_result_t platform_sendfile(socket_t out_fd, int in_fd,
                                     off_t *offset, size_t count,
                                     size_t *bytes_sent) {
    off_t len = (off_t)count;
    off_t start = offset ? *offset : 0;
    
    int ret = sendfile(in_fd, out_fd, start, &len, NULL, 0);
    
    *bytes_sent = (size_t)len;
    
    if (ret == 0) {
        if (offset) *offset += len;
        return SENDFILE_OK;
    }
    
    if (errno == EAGAIN) {
        if (offset) *offset += len;
        return SENDFILE_AGAIN;
    }
    
    return SENDFILE_ERROR;
}

bool platform_has_sendfile_support(void) {
    return true;
}

const char *platform_sendfile_method(void) {
    return "sendfile (macOS)";
}

#elif defined(ZIRCON_FREEBSD)

sendfile_result_t platform_sendfile(socket_t out_fd, int in_fd,
                                     off_t *offset, size_t count,
                                     size_t *bytes_sent) {
    off_t sbytes = 0;
    off_t start = offset ? *offset : 0;
    
    int ret = sendfile(in_fd, out_fd, start, count, NULL, &sbytes, 0);
    
    *bytes_sent = (size_t)sbytes;
    
    if (ret == 0) {
        if (offset) *offset += sbytes;
        return SENDFILE_OK;
    }
    
    if (errno == EAGAIN) {
        if (offset) *offset += sbytes;
        return SENDFILE_AGAIN;
    }
    
    return SENDFILE_ERROR;
}

bool platform_has_sendfile_support(void) {
    return true;
}

const char *platform_sendfile_method(void) {
    return "sendfile (FreeBSD)";
}

#elif defined(ZIRCON_WINDOWS)

sendfile_result_t platform_sendfile(socket_t out_fd, int in_fd,
                                     off_t *offset, size_t count,
                                     size_t *bytes_sent) {
    HANDLE hFile = (HANDLE)_get_osfhandle(in_fd);
    if (hFile == INVALID_HANDLE_VALUE) {
        *bytes_sent = 0;
        return SENDFILE_ERROR;
    }
    
    OVERLAPPED ov = {0};
    if (offset) {
        ov.Offset = (DWORD)(*offset & 0xFFFFFFFF);
        ov.OffsetHigh = (DWORD)((*offset >> 32) & 0xFFFFFFFF);
    }
    
    if (TransmitFile((SOCKET)out_fd, hFile, (DWORD)count, 0, &ov, NULL, 0)) {
        *bytes_sent = count;
        if (offset) *offset += count;
        return SENDFILE_OK;
    }
    
    DWORD err = WSAGetLastError();
    if (err == WSA_IO_PENDING || err == WSAEWOULDBLOCK) {
        *bytes_sent = 0;
        return SENDFILE_AGAIN;
    }
    
    *bytes_sent = 0;
    return SENDFILE_ERROR;
}

bool platform_has_sendfile_support(void) {
    return true;
}

const char *platform_sendfile_method(void) {
    return "TransmitFile (Windows)";
}

#else

sendfile_result_t platform_sendfile(socket_t out_fd, int in_fd,
                                     off_t *offset, size_t count,
                                     size_t *bytes_sent) {
    return sendfile_userspace_fallback(out_fd, in_fd, offset, count, bytes_sent);
}

bool platform_has_sendfile_support(void) {
    return false;
}

const char *platform_sendfile_method(void) {
    return "userspace copy (fallback)";
}

#endif
