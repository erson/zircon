#ifndef SECURITY_HEADERS_H
#define SECURITY_HEADERS_H

#include <stddef.h>

/* Add security headers to response */
void add_security_headers(char *headers, size_t size);

#endif /* SECURITY_HEADERS_H */
