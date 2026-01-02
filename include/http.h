#ifndef HTTP_H
#define HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

typedef enum {
    HTTP_GET,
    HTTP_HEAD,
    HTTP_POST,
    HTTP_UNSUPPORTED
} http_method_t;

typedef struct {
    http_method_t method;
    char path[256];
    char version[16];
} http_request_t;

bool http_parse_request(const char *buffer, size_t length, http_request_t *req);

const char *http_get_mime_type(const char *path);

char *http_generate_etag(time_t mtime, size_t size);

bool http_check_etag_match(const char *request, const char *etag);

#endif /* HTTP_H */
