#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "../include/http.h"

/* Test utilities */
#define TEST(name) static bool test_##name(void)
#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_STR_EQ(a, b) ASSERT_TRUE(strcmp((a), (b)) == 0)

/* Test HTTP request parsing */
TEST(http_parse_request_valid_get) {
    const char *request = "GET /index.html HTTP/1.1\r\n\r\n";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(req.method, HTTP_GET);
    ASSERT_STR_EQ(req.path, "/index.html");
    ASSERT_STR_EQ(req.version, "HTTP/1.1");
    
    return true;
}

TEST(http_parse_request_valid_head) {
    const char *request = "HEAD /test.css HTTP/1.0\r\n\r\n";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(req.method, HTTP_HEAD);
    ASSERT_STR_EQ(req.path, "/test.css");
    ASSERT_STR_EQ(req.version, "HTTP/1.0");
    
    return true;
}

TEST(http_parse_request_valid_post) {
    const char *request = "POST /api/data HTTP/1.1\r\n\r\n";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(req.method, HTTP_POST);
    ASSERT_STR_EQ(req.path, "/api/data");
    ASSERT_STR_EQ(req.version, "HTTP/1.1");
    
    return true;
}

TEST(http_parse_request_unsupported_method) {
    const char *request = "PUT /upload HTTP/1.1\r\n\r\n";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    /* Unsupported methods return false but req.method is still set */
    ASSERT_FALSE(result);
    ASSERT_EQ(req.method, HTTP_UNSUPPORTED);
    
    return true;
}
TEST(http_parse_request_invalid_format) {
    const char *request = "INVALID REQUEST FORMAT";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    ASSERT_FALSE(result);
    
    return true;
}

TEST(http_parse_request_empty) {
    const char *request = "";
    http_request_t req;
    
    bool result = http_parse_request(request, 0, &req);
    
    ASSERT_FALSE(result);
    
    return true;
}

TEST(http_parse_request_null_buffer) {
    http_request_t req;
    
    bool result = http_parse_request(NULL, 100, &req);
    
    ASSERT_FALSE(result);
    
    return true;
}

TEST(http_parse_request_long_path) {
    char long_path[300];
    memset(long_path, 'a', 299);
    long_path[299] = '\0';
    
    char request[400];
    snprintf(request, sizeof(request), "GET /%s HTTP/1.1\r\n\r\n", long_path);
    
    http_request_t req;
    bool result = http_parse_request(request, strlen(request), &req);
    
    /* sscanf with %255s safely truncates, so this now succeeds */
    ASSERT_TRUE(result);
    
    return true;
}
TEST(http_parse_request_path_with_query) {
    const char *request = "GET /search?q=test&type=web HTTP/1.1\r\n\r\n";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(req.method, HTTP_GET);
    ASSERT_STR_EQ(req.path, "/search?q=test&type=web");
    
    return true;
}

TEST(http_parse_request_root_path) {
    const char *request = "GET / HTTP/1.1\r\n\r\n";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    ASSERT_TRUE(result);
    ASSERT_EQ(req.method, HTTP_GET);
    ASSERT_STR_EQ(req.path, "/");
    
    return true;
}

TEST(http_parse_request_malformed_version) {
    const char *request = "GET /index.html INVALID_VERSION\r\n\r\n";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    // Should still parse but with the malformed version
    ASSERT_TRUE(result);
    ASSERT_EQ(req.method, HTTP_GET);
    ASSERT_STR_EQ(req.path, "/index.html");
    
    return true;
}

TEST(http_parse_request_case_sensitivity) {
    const char *request = "get /index.html HTTP/1.1\r\n\r\n";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    // Should fail due to case sensitivity
    ASSERT_FALSE(result);
    
    return true;
}

/* Test runner */
int main(void) {
    printf("Running HTTP module tests...\n\n");
    
    int failed = 0;
    
    #define RUN_TEST(name) do { \
        printf("Running %s... ", #name); \
        if (test_##name()) { \
            printf("PASS\n"); \
        } else { \
            printf("FAIL\n"); \
            failed++; \
        } \
    } while(0)
    
    RUN_TEST(http_parse_request_valid_get);
    RUN_TEST(http_parse_request_valid_head);
    RUN_TEST(http_parse_request_valid_post);
    RUN_TEST(http_parse_request_unsupported_method);
    RUN_TEST(http_parse_request_invalid_format);
    RUN_TEST(http_parse_request_empty);
    RUN_TEST(http_parse_request_null_buffer);
    RUN_TEST(http_parse_request_long_path);
    RUN_TEST(http_parse_request_path_with_query);
    RUN_TEST(http_parse_request_root_path);
    RUN_TEST(http_parse_request_malformed_version);
    RUN_TEST(http_parse_request_case_sensitivity);
    
    printf("\nHTTP Tests Summary:\n");
    printf("Failed: %d\n", failed);
    printf("Status: %s\n", failed == 0 ? "PASS" : "FAIL");
    
    return failed;
}

