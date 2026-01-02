#include "security_headers.h"
#include <string.h>
#include <stdio.h>

void add_security_headers(char *headers, size_t size) {
    snprintf(headers + strlen(headers), size - strlen(headers),
        "X-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: DENY\r\n"
        "X-XSS-Protection: 1; mode=block\r\n"
        "Content-Security-Policy: default-src 'self'\r\n"
        "Referrer-Policy: strict-origin-when-cross-origin\r\n"
        "Permissions-Policy: camera=(), microphone=(), geolocation=()\r\n");
}
