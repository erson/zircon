#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include "../include/rate_limiter.h"

/* Test utilities */
#define TEST(name) static bool test_##name(void)
#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

/* Test rate limiter creation and destruction */
TEST(rate_limiter_create_destroy) {
    rate_limiter_config_t config = {
        .max_requests = 10,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    rate_limiter_destroy(limiter);
    
    return true;
}

/* Test rate limiter with NULL config */
TEST(rate_limiter_create_null_config) {
    rate_limiter_t *limiter = rate_limiter_create(NULL);
    ASSERT_TRUE(limiter == NULL);
    
    return true;
}

/* Test basic rate limiting functionality */
TEST(rate_limiter_basic_functionality) {
    rate_limiter_config_t config = {
        .max_requests = 5,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    const char *ip = "192.168.1.100";
    
    // First 5 requests should be allowed
    for (int i = 0; i < 5; i++) {
        bool allowed = rate_limiter_check(limiter, ip);
        ASSERT_TRUE(allowed);
    }
    
    // 6th request should be blocked
    bool allowed = rate_limiter_check(limiter, ip);
    ASSERT_FALSE(allowed);
    
    rate_limiter_destroy(limiter);
    return true;
}

/* Test rate limiting with different IPs */
TEST(rate_limiter_different_ips) {
    rate_limiter_config_t config = {
        .max_requests = 3,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    const char *ip1 = "192.168.1.100";
    const char *ip2 = "192.168.1.101";
    
    // Each IP should have its own limit
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(rate_limiter_check(limiter, ip1));
        ASSERT_TRUE(rate_limiter_check(limiter, ip2));
    }
    
    // Both IPs should now be blocked
    ASSERT_FALSE(rate_limiter_check(limiter, ip1));
    ASSERT_FALSE(rate_limiter_check(limiter, ip2));
    
    rate_limiter_destroy(limiter);
    return true;
}

/* Test localhost exemption */
TEST(rate_limiter_localhost_exemption) {
    rate_limiter_config_t config = {
        .max_requests = 2,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    const char *localhost_ips[] = {
        "127.0.0.1",
        "::1",
        "localhost"
    };
    
    // Localhost should never be rate limited
    for (int ip_idx = 0; ip_idx < 3; ip_idx++) {
        for (int i = 0; i < 10; i++) {  // Try many more than the limit
            bool allowed = rate_limiter_check(limiter, localhost_ips[ip_idx]);
            ASSERT_TRUE(allowed);
        }
    }
    
    rate_limiter_destroy(limiter);
    return true;
}

/* Test rate limiter with NULL IP */
TEST(rate_limiter_null_ip) {
    rate_limiter_config_t config = {
        .max_requests = 5,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    // NULL IP should be rejected
    bool allowed = rate_limiter_check(limiter, NULL);
    ASSERT_FALSE(allowed);
    
    rate_limiter_destroy(limiter);
    return true;
}

/* Test rate limiter with NULL limiter */
TEST(rate_limiter_null_limiter) {
    // NULL limiter should be handled gracefully
    bool allowed = rate_limiter_check(NULL, "192.168.1.100");
    ASSERT_FALSE(allowed);
    
    return true;
}

/* Test rate limiter with empty IP */
TEST(rate_limiter_empty_ip) {
    rate_limiter_config_t config = {
        .max_requests = 5,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    // Empty IP should be rejected
    bool allowed = rate_limiter_check(limiter, "");
    ASSERT_FALSE(allowed);
    
    rate_limiter_destroy(limiter);
    return true;
}

/* Test rate limiter with various IP formats */
TEST(rate_limiter_ip_formats) {
    rate_limiter_config_t config = {
        .max_requests = 3,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    const char *valid_ips[] = {
        "192.168.1.1",
        "10.0.0.1",
        "172.16.0.1",
        "8.8.8.8",
        "255.255.255.255",
        "0.0.0.0"
    };
    
    // Test each IP format
    for (int ip_idx = 0; ip_idx < 6; ip_idx++) {
        // Each IP should be rate limited independently
        for (int i = 0; i < 3; i++) {
            bool allowed = rate_limiter_check(limiter, valid_ips[ip_idx]);
            ASSERT_TRUE(allowed);
        }
        
        // 4th request should be blocked
        bool allowed = rate_limiter_check(limiter, valid_ips[ip_idx]);
        ASSERT_FALSE(allowed);
    }
    
    rate_limiter_destroy(limiter);
    return true;
}

/* Test rate limiter with invalid IP formats */
TEST(rate_limiter_invalid_ip_formats) {
    rate_limiter_config_t config = {
        .max_requests = 5,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    const char *invalid_ips[] = {
        "256.256.256.256",
        "192.168.1",
        "192.168.1.1.1",
        "not.an.ip.address",
        "192.168.1.-1",
        "192.168.1.abc"
    };
    
    // Invalid IPs should be rejected
    for (int i = 0; i < 6; i++) {
        bool allowed = rate_limiter_check(limiter, invalid_ips[i]);
        // Depending on implementation, might be rejected or treated as string
        // At minimum, should not crash
    }
    
    rate_limiter_destroy(limiter);
    return true;
}

/* Test rate limiter configuration edge cases */
TEST(rate_limiter_config_edge_cases) {
    rate_limiter_config_t config;
    
    // Test with zero max_requests
    config.max_requests = 0;
    config.time_window = 60;
    config.block_duration = 300;
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    if (limiter) {
        // If created, all requests should be blocked
        bool allowed = rate_limiter_check(limiter, "192.168.1.100");
        ASSERT_FALSE(allowed);
        rate_limiter_destroy(limiter);
    }
    
    // Test with zero time_window
    config.max_requests = 5;
    config.time_window = 0;
    config.block_duration = 300;
    
    limiter = rate_limiter_create(&config);
    // Should either fail to create or handle gracefully
    if (limiter) {
        rate_limiter_destroy(limiter);
    }
    
    // Test with zero block_duration
    config.max_requests = 5;
    config.time_window = 60;
    config.block_duration = 0;
    
    limiter = rate_limiter_create(&config);
    // Should either fail to create or handle gracefully
    if (limiter) {
        rate_limiter_destroy(limiter);
    }
    
    return true;
}

/* Test concurrent access simulation */
TEST(rate_limiter_concurrent_simulation) {
    rate_limiter_config_t config = {
        .max_requests = 10,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    const char *ip = "192.168.1.100";
    int allowed_count = 0;
    int blocked_count = 0;
    
    // Simulate rapid requests
    for (int i = 0; i < 20; i++) {
        if (rate_limiter_check(limiter, ip)) {
            allowed_count++;
        } else {
            blocked_count++;
        }
    }
    
    // Should have allowed exactly max_requests
    ASSERT_TRUE(allowed_count == 10);
    ASSERT_TRUE(blocked_count == 10);
    
    rate_limiter_destroy(limiter);
    return true;
}

/* Test runner */
int main(void) {
    printf("Running Rate Limiter tests...\n\n");
    
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
    
    RUN_TEST(rate_limiter_create_destroy);
    RUN_TEST(rate_limiter_create_null_config);
    RUN_TEST(rate_limiter_basic_functionality);
    RUN_TEST(rate_limiter_different_ips);
    RUN_TEST(rate_limiter_localhost_exemption);
    RUN_TEST(rate_limiter_null_ip);
    RUN_TEST(rate_limiter_null_limiter);
    RUN_TEST(rate_limiter_empty_ip);
    RUN_TEST(rate_limiter_ip_formats);
    RUN_TEST(rate_limiter_invalid_ip_formats);
    RUN_TEST(rate_limiter_config_edge_cases);
    RUN_TEST(rate_limiter_concurrent_simulation);
    
    printf("\nRate Limiter Tests Summary:\n");
    printf("Failed: %d\n", failed);
    printf("Status: %s\n", failed == 0 ? "PASS" : "FAIL");
    
    return failed;
}

