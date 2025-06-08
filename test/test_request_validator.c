#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/request_validator.h"
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

/* Test path safety validation */
TEST(is_path_safe_normal_paths) {
    ASSERT_TRUE(is_path_safe("/index.html"));
    ASSERT_TRUE(is_path_safe("/css/style.css"));
    ASSERT_TRUE(is_path_safe("/js/script.js"));
    ASSERT_TRUE(is_path_safe("/images/logo.png"));
    ASSERT_TRUE(is_path_safe("/"));
    
    return true;
}

TEST(is_path_safe_path_traversal_attacks) {
    ASSERT_FALSE(is_path_safe("../etc/passwd"));
    ASSERT_FALSE(is_path_safe("/../../etc/passwd"));
    ASSERT_FALSE(is_path_safe("/../../../etc/shadow"));
    ASSERT_FALSE(is_path_safe("/dir/../../../etc/passwd"));
    ASSERT_FALSE(is_path_safe("/normal/path/../../etc/passwd"));
    
    return true;
}

TEST(is_path_safe_encoded_traversal) {
    ASSERT_FALSE(is_path_safe("/%2e%2e/etc/passwd"));
    ASSERT_FALSE(is_path_safe("/%2e%2e%2f%2e%2e/etc/passwd"));
    ASSERT_FALSE(is_path_safe("/..%2fetc%2fpasswd"));
    ASSERT_FALSE(is_path_safe("/%252e%252e/etc/passwd"));
    
    return true;
}

TEST(is_path_safe_null_bytes) {
    char path_with_null[] = "/index.html\0../etc/passwd";
    ASSERT_FALSE(is_path_safe(path_with_null));
    
    return true;
}

TEST(is_path_safe_long_paths) {
    char long_path[1000];
    memset(long_path, 'a', 999);
    long_path[0] = '/';
    long_path[999] = '\0';
    
    // Very long paths should be rejected
    ASSERT_FALSE(is_path_safe(long_path));
    
    return true;
}

TEST(is_path_safe_special_characters) {
    ASSERT_TRUE(is_path_safe("/file-name.html"));
    ASSERT_TRUE(is_path_safe("/file_name.html"));
    ASSERT_TRUE(is_path_safe("/file.name.html"));
    ASSERT_TRUE(is_path_safe("/123file.html"));
    
    // Some special characters should be allowed
    ASSERT_TRUE(is_path_safe("/file%20name.html")); // URL encoded space
    
    return true;
}

TEST(is_path_safe_empty_and_null) {
    ASSERT_FALSE(is_path_safe(""));
    ASSERT_FALSE(is_path_safe(NULL));
    
    return true;
}

/* Test HTTP method validation */
TEST(is_method_allowed_valid_methods) {
    ASSERT_TRUE(is_method_allowed(HTTP_GET));
    ASSERT_TRUE(is_method_allowed(HTTP_HEAD));
    ASSERT_TRUE(is_method_allowed(HTTP_POST));
    
    return true;
}

TEST(is_method_allowed_invalid_methods) {
    ASSERT_FALSE(is_method_allowed(HTTP_UNSUPPORTED));
    
    return true;
}

/* Test complete request validation */
TEST(validate_request_valid_requests) {
    http_request_t req;
    
    // Valid GET request
    req.method = HTTP_GET;
    strcpy(req.path, "/index.html");
    strcpy(req.version, "HTTP/1.1");
    
    validation_result_t result = validate_request(&req);
    ASSERT_EQ(result.is_valid, true);
    ASSERT_EQ(result.status_code, 200);
    
    // Valid HEAD request
    req.method = HTTP_HEAD;
    strcpy(req.path, "/style.css");
    
    result = validate_request(&req);
    ASSERT_EQ(result.is_valid, true);
    ASSERT_EQ(result.status_code, 200);
    
    return true;
}

TEST(validate_request_path_traversal) {
    http_request_t req;
    
    req.method = HTTP_GET;
    strcpy(req.path, "../etc/passwd");
    strcpy(req.version, "HTTP/1.1");
    
    validation_result_t result = validate_request(&req);
    ASSERT_EQ(result.is_valid, false);
    ASSERT_EQ(result.status_code, 403);
    
    return true;
}

TEST(validate_request_unsupported_method) {
    http_request_t req;
    
    req.method = HTTP_UNSUPPORTED;
    strcpy(req.path, "/index.html");
    strcpy(req.version, "HTTP/1.1");
    
    validation_result_t result = validate_request(&req);
    ASSERT_EQ(result.is_valid, false);
    ASSERT_EQ(result.status_code, 405); // Method Not Allowed
    
    return true;
}

TEST(validate_request_null_request) {
    validation_result_t result = validate_request(NULL);
    ASSERT_EQ(result.is_valid, false);
    ASSERT_EQ(result.status_code, 400); // Bad Request
    
    return true;
}

TEST(validate_request_restricted_file_types) {
    http_request_t req;
    
    req.method = HTTP_GET;
    strcpy(req.version, "HTTP/1.1");
    
    // Test various restricted file types
    const char *restricted_files[] = {
        "/script.php",
        "/config.ini",
        "/database.db",
        "/backup.bak",
        "/log.log",
        "/.htaccess",
        "/web.config"
    };
    
    for (int i = 0; i < sizeof(restricted_files) / sizeof(restricted_files[0]); i++) {
        strcpy(req.path, restricted_files[i]);
        validation_result_t result = validate_request(&req);
        ASSERT_EQ(result.is_valid, false);
        ASSERT_EQ(result.status_code, 403);
    }
    
    return true;
}

TEST(validate_request_allowed_file_types) {
    http_request_t req;
    
    req.method = HTTP_GET;
    strcpy(req.version, "HTTP/1.1");
    
    // Test various allowed file types
    const char *allowed_files[] = {
        "/index.html",
        "/style.css",
        "/script.js",
        "/image.png",
        "/image.jpg",
        "/image.gif",
        "/document.pdf",
        "/data.json",
        "/feed.xml"
    };
    
    for (int i = 0; i < sizeof(allowed_files) / sizeof(allowed_files[0]); i++) {
        strcpy(req.path, allowed_files[i]);
        validation_result_t result = validate_request(&req);
        ASSERT_EQ(result.is_valid, true);
        ASSERT_EQ(result.status_code, 200);
    }
    
    return true;
}

/* Test runner */
int main(void) {
    printf("Running Request Validator tests...\n\n");
    
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
    
    RUN_TEST(is_path_safe_normal_paths);
    RUN_TEST(is_path_safe_path_traversal_attacks);
    RUN_TEST(is_path_safe_encoded_traversal);
    RUN_TEST(is_path_safe_null_bytes);
    RUN_TEST(is_path_safe_long_paths);
    RUN_TEST(is_path_safe_special_characters);
    RUN_TEST(is_path_safe_empty_and_null);
    RUN_TEST(is_method_allowed_valid_methods);
    RUN_TEST(is_method_allowed_invalid_methods);
    RUN_TEST(validate_request_valid_requests);
    RUN_TEST(validate_request_path_traversal);
    RUN_TEST(validate_request_unsupported_method);
    RUN_TEST(validate_request_null_request);
    RUN_TEST(validate_request_restricted_file_types);
    RUN_TEST(validate_request_allowed_file_types);
    
    printf("\nRequest Validator Tests Summary:\n");
    printf("Failed: %d\n", failed);
    printf("Status: %s\n", failed == 0 ? "PASS" : "FAIL");
    
    return failed;
}

