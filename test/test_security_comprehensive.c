#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/security.h"
#include "../include/security_config.h"

/* Test utilities */
#define TEST(name) static bool test_##name(void)
#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

/* Common XSS attack payloads */
static const char *xss_payloads[] = {
    "<script>alert('XSS')</script>",
    "<img src=x onerror=alert('XSS')>",
    "<svg onload=alert('XSS')>",
    "javascript:alert('XSS')",
    "<iframe src=javascript:alert('XSS')>",
    "<body onload=alert('XSS')>",
    "<input onfocus=alert('XSS') autofocus>",
    "<select onfocus=alert('XSS') autofocus>",
    "<textarea onfocus=alert('XSS') autofocus>",
    "<keygen onfocus=alert('XSS') autofocus>",
    "<video><source onerror=alert('XSS')>",
    "<audio src=x onerror=alert('XSS')>",
    "<details open ontoggle=alert('XSS')>",
    "<marquee onstart=alert('XSS')>",
    "<%2fscript%3ealert('XSS')%3c%2fscript%3e",
    "<script>eval(String.fromCharCode(97,108,101,114,116,40,39,88,83,83,39,41))</script>",
    "<img src=\"javascript:alert('XSS')\">",
    "<div style=\"background-image:url(javascript:alert('XSS'))\">",
    "<link rel=stylesheet href=\"javascript:alert('XSS')\">",
    "<object data=\"javascript:alert('XSS')\">",
    "<embed src=\"javascript:alert('XSS')\">",
    "<applet code=\"javascript:alert('XSS')\">",
    "<meta http-equiv=\"refresh\" content=\"0;url=javascript:alert('XSS')\">",
    "<form action=\"javascript:alert('XSS')\">",
    "<isindex action=\"javascript:alert('XSS')\">",
    "<table background=\"javascript:alert('XSS')\">",
    "<td background=\"javascript:alert('XSS')\">",
    "<div style=\"width:expression(alert('XSS'))\">",
    "<style>@import'javascript:alert(\"XSS\")';</style>",
    "<IMG SRC=&#106;&#97;&#118;&#97;&#115;&#99;&#114;&#105;&#112;&#116;&#58;&#97;&#108;&#101;&#114;&#116;&#40;&#39;&#88;&#83;&#83;&#39;&#41;>",
    NULL
};

/* Common SQL injection payloads */
static const char *sql_injection_payloads[] = {
    "' OR '1'='1",
    "' OR 1=1--",
    "' OR 1=1#",
    "' OR 1=1/*",
    "') OR '1'='1--",
    "') OR ('1'='1--",
    "1' OR '1'='1",
    "1' OR 1=1--",
    "1' OR 1=1#",
    "1' OR 1=1/*",
    "admin'--",
    "admin'#",
    "admin'/*",
    "' OR 1=1 LIMIT 1--",
    "' UNION SELECT 1,2,3--",
    "' UNION ALL SELECT 1,2,3--",
    "'; DROP TABLE users--",
    "'; DELETE FROM users--",
    "'; INSERT INTO users--",
    "'; UPDATE users SET--",
    "1; DROP TABLE users--",
    "1; DELETE FROM users--",
    "1; INSERT INTO users--",
    "1; UPDATE users SET--",
    "' AND (SELECT COUNT(*) FROM users) > 0--",
    "' AND (SELECT SUBSTRING(@@version,1,1)) = '5'--",
    "' AND (SELECT SUBSTRING(user(),1,1)) = 'r'--",
    "' AND (SELECT SUBSTRING(database(),1,1)) = 'a'--",
    "' WAITFOR DELAY '00:00:05'--",
    "'; WAITFOR DELAY '00:00:05'--",
    "1' WAITFOR DELAY '00:00:05'--",
    "1'; WAITFOR DELAY '00:00:05'--",
    NULL
};

/* Path traversal payloads */
static const char *path_traversal_payloads[] = {
    "../",
    "..\\",
    "..%2f",
    "..%5c",
    "..%252f",
    "..%255c",
    "%2e%2e%2f",
    "%2e%2e%5c",
    "%252e%252e%252f",
    "%252e%252e%255c",
    "....//",
    "....\\\\",
    "..../",
    "....\\",
    "../../../etc/passwd",
    "..\\..\\..\\windows\\system32\\config\\sam",
    "%2e%2e%2f%2e%2e%2f%2e%2e%2fetc%2fpasswd",
    "%252e%252e%252f%252e%252e%252f%252e%252e%252fetc%252fpasswd",
    "....//....//....//etc/passwd",
    "....\\\\....\\\\....\\\\windows\\system32\\config\\sam",
    NULL
};

/* Test XSS detection */
TEST(security_xss_detection) {
    security_config_t config;
    security_config_set_defaults(&config);
    config.xss_protection = true;
    
    security_ctx_t *ctx = security_create(&config);
    ASSERT_TRUE(ctx != NULL);
    
    // Test each XSS payload
    for (int i = 0; xss_payloads[i] != NULL; i++) {
        bool result = security_check_request(ctx, "127.0.0.1", "GET", "/", 
                                           xss_payloads[i], xss_payloads[i], 
                                           strlen(xss_payloads[i]));
        if (result) {
            printf("XSS payload not detected: %s\n", xss_payloads[i]);
            security_destroy(ctx);
            return false;
        }
    }
    
    security_destroy(ctx);
    return true;
}

/* Test SQL injection detection */
TEST(security_sql_injection_detection) {
    security_config_t config;
    security_config_set_defaults(&config);
    config.sql_injection_protection = true;
    
    security_ctx_t *ctx = security_create(&config);
    ASSERT_TRUE(ctx != NULL);
    
    // Test each SQL injection payload
    for (int i = 0; sql_injection_payloads[i] != NULL; i++) {
        bool result = security_check_request(ctx, "127.0.0.1", "GET", "/", 
                                           sql_injection_payloads[i], 
                                           sql_injection_payloads[i], 
                                           strlen(sql_injection_payloads[i]));
        if (result) {
            printf("SQL injection payload not detected: %s\n", sql_injection_payloads[i]);
            security_destroy(ctx);
            return false;
        }
    }
    
    security_destroy(ctx);
    return true;
}

/* Test path traversal detection */
TEST(security_path_traversal_detection) {
    security_config_t config;
    security_config_set_defaults(&config);
    config.path_traversal_protection = true;
    
    security_ctx_t *ctx = security_create(&config);
    ASSERT_TRUE(ctx != NULL);
    
    // Test each path traversal payload
    for (int i = 0; path_traversal_payloads[i] != NULL; i++) {
        char path[256];
        snprintf(path, sizeof(path), "/%s", path_traversal_payloads[i]);
        
        bool result = security_check_request(ctx, "127.0.0.1", "GET", path, 
                                           "", "", 0);
        if (result) {
            printf("Path traversal payload not detected: %s\n", path);
            security_destroy(ctx);
            return false;
        }
    }
    
    security_destroy(ctx);
    return true;
}

/* Test legitimate requests are allowed */
TEST(security_legitimate_requests) {
    security_config_t config;
    security_config_set_defaults(&config);
    
    security_ctx_t *ctx = security_create(&config);
    ASSERT_TRUE(ctx != NULL);
    
    // Test legitimate requests
    const char *legitimate_requests[][5] = {
        {"127.0.0.1", "GET", "/index.html", "", ""},
        {"127.0.0.1", "GET", "/css/style.css", "", ""},
        {"127.0.0.1", "GET", "/js/script.js", "", ""},
        {"127.0.0.1", "GET", "/images/logo.png", "", ""},
        {"127.0.0.1", "POST", "/api/data", "name=John&age=30", "name=John&age=30"},
        {"127.0.0.1", "GET", "/search", "q=hello world", ""},
        {"192.168.1.100", "GET", "/about.html", "", ""},
        {"10.0.0.1", "HEAD", "/favicon.ico", "", ""},
        {NULL, NULL, NULL, NULL, NULL}
    };
    
    for (int i = 0; legitimate_requests[i][0] != NULL; i++) {
        bool result = security_check_request(ctx, 
                                           legitimate_requests[i][0],  // IP
                                           legitimate_requests[i][1],  // method
                                           legitimate_requests[i][2],  // path
                                           legitimate_requests[i][3],  // query
                                           legitimate_requests[i][4],  // body
                                           strlen(legitimate_requests[i][4]));
        if (!result) {
            printf("Legitimate request blocked: %s %s %s\n", 
                   legitimate_requests[i][1], 
                   legitimate_requests[i][2],
                   legitimate_requests[i][3]);
            security_destroy(ctx);
            return false;
        }
    }
    
    security_destroy(ctx);
    return true;
}

/* Test mixed attack vectors */
TEST(security_mixed_attack_vectors) {
    security_config_t config;
    security_config_set_defaults(&config);
    
    security_ctx_t *ctx = security_create(&config);
    ASSERT_TRUE(ctx != NULL);
    
    // Test combinations of attack vectors
    const char *mixed_attacks[][5] = {
        {"127.0.0.1", "GET", "/../etc/passwd", "<script>alert('xss')</script>", ""},
        {"127.0.0.1", "POST", "/login", "", "username=admin'--&password=<script>alert('xss')</script>"},
        {"127.0.0.1", "GET", "/search", "q=' OR 1=1--", ""},
        {"127.0.0.1", "GET", "/%2e%2e%2fetc%2fpasswd", "q=<img src=x onerror=alert('xss')>", ""},
        {NULL, NULL, NULL, NULL, NULL}
    };
    
    for (int i = 0; mixed_attacks[i][0] != NULL; i++) {
        bool result = security_check_request(ctx, 
                                           mixed_attacks[i][0],  // IP
                                           mixed_attacks[i][1],  // method
                                           mixed_attacks[i][2],  // path
                                           mixed_attacks[i][3],  // query
                                           mixed_attacks[i][4],  // body
                                           strlen(mixed_attacks[i][4]));
        if (result) {
            printf("Mixed attack not detected: %s %s %s %s %s\n", 
                   mixed_attacks[i][0], mixed_attacks[i][1], 
                   mixed_attacks[i][2], mixed_attacks[i][3], mixed_attacks[i][4]);
            security_destroy(ctx);
            return false;
        }
    }
    
    security_destroy(ctx);
    return true;
}

/* Test security configuration */
TEST(security_config_validation) {
    security_config_t config;
    
    // Test default configuration
    security_config_set_defaults(&config);
    ASSERT_TRUE(config.xss_protection);
    ASSERT_TRUE(config.sql_injection_protection);
    ASSERT_TRUE(config.path_traversal_protection);
    
    // Test creating security context with valid config
    security_ctx_t *ctx = security_create(&config);
    ASSERT_TRUE(ctx != NULL);
    security_destroy(ctx);
    
    // Test creating security context with NULL config
    ctx = security_create(NULL);
    ASSERT_TRUE(ctx == NULL);
    
    return true;
}

/* Test edge cases */
TEST(security_edge_cases) {
    security_config_t config;
    security_config_set_defaults(&config);
    
    security_ctx_t *ctx = security_create(&config);
    ASSERT_TRUE(ctx != NULL);
    
    // Test with NULL parameters
    bool result = security_check_request(NULL, "127.0.0.1", "GET", "/", "", "", 0);
    ASSERT_FALSE(result);
    
    result = security_check_request(ctx, NULL, "GET", "/", "", "", 0);
    ASSERT_FALSE(result);
    
    result = security_check_request(ctx, "127.0.0.1", NULL, "/", "", "", 0);
    ASSERT_FALSE(result);
    
    result = security_check_request(ctx, "127.0.0.1", "GET", NULL, "", "", 0);
    ASSERT_FALSE(result);
    
    // Test with empty strings
    result = security_check_request(ctx, "", "", "", "", "", 0);
    ASSERT_FALSE(result);
    
    // Test with very long inputs
    char long_input[10000];
    memset(long_input, 'A', sizeof(long_input) - 1);
    long_input[sizeof(long_input) - 1] = '\0';
    
    result = security_check_request(ctx, "127.0.0.1", "GET", long_input, "", "", 0);
    ASSERT_FALSE(result);
    
    security_destroy(ctx);
    return true;
}

/* Test runner */
int main(void) {
    printf("Running Comprehensive Security tests...\n\n");
    
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
    
    RUN_TEST(security_xss_detection);
    RUN_TEST(security_sql_injection_detection);
    RUN_TEST(security_path_traversal_detection);
    RUN_TEST(security_legitimate_requests);
    RUN_TEST(security_mixed_attack_vectors);
    RUN_TEST(security_config_validation);
    RUN_TEST(security_edge_cases);
    
    printf("\nComprehensive Security Tests Summary:\n");
    printf("Failed: %d\n", failed);
    printf("Status: %s\n", failed == 0 ? "PASS" : "FAIL");
    
    return failed;
}

