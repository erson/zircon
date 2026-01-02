#ifndef PLATFORM_SENDFILE_H
#define PLATFORM_SENDFILE_H

#include "platform/detect.h"
#include "platform/compat.h"
#include <sys/types.h>

typedef enum {
    SENDFILE_OK = 0,
    SENDFILE_AGAIN = -1,
    SENDFILE_ERROR = -2,
    SENDFILE_UNSUPPORTED = -3
} sendfile_result_t;

ZIRCON_API sendfile_result_t platform_sendfile(socket_t out_fd, int in_fd,
                                                off_t *offset, size_t count,
                                                size_t *bytes_sent);

ZIRCON_API bool platform_has_sendfile_support(void);

ZIRCON_API const char *platform_sendfile_method(void);

#endif /* PLATFORM_SENDFILE_H */
