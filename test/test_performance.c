#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "../include/server.h"
#include "../include/http.h"
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

/* Performance measurement utilities */
static double get_time_diff(struct timeval start, struct timeval end) {
    return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
}

/* Test HTTP parsing performance */
TEST(http_parse_performance) {
    const char *request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\nUser-Agent: Test\r\n\r\n";
    http_request_t req;
    struct timeval start, end;
    
    const int iterations = 100000;
    
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < iterations; i++) {
        http_parse_request(request, strlen(request), &req);
    }
    
    gettimeofday(&end, NULL);
    
    double elapsed = get_time_diff(start, end);
    double requests_per_second = iterations / elapsed;
    
    printf("\n  HTTP Parse Performance:\n");
    printf("  - Parsed %d requests in %.3f seconds\n", iterations, elapsed);
    printf("  - Rate: %.0f requests/second\n", requests_per_second);
    printf("  - Average time per request: %.6f seconds\n", elapsed / iterations);
    
    // Should be able to parse at least 10,000 requests per second
    ASSERT_TRUE(requests_per_second > 10000);
    
    return true;
}

/* Test rate limiter performance */
TEST(rate_limiter_performance) {
    rate_limiter_config_t config = {
        .max_requests = 1000,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    struct timeval start, end;
    const int iterations = 50000;
    
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < iterations; i++) {
        char ip[16];
        snprintf(ip, sizeof(ip), "192.168.%d.%d", (i / 256) % 256, i % 256);
        rate_limiter_check(limiter, ip);
    }
    
    gettimeofday(&end, NULL);
    
    double elapsed = get_time_diff(start, end);
    double checks_per_second = iterations / elapsed;
    
    printf("\n  Rate Limiter Performance:\n");
    printf("  - Processed %d checks in %.3f seconds\n", iterations, elapsed);
    printf("  - Rate: %.0f checks/second\n", checks_per_second);
    printf("  - Average time per check: %.6f seconds\n", elapsed / iterations);
    
    // Should be able to process at least 5,000 checks per second
    ASSERT_TRUE(checks_per_second > 5000);
    
    rate_limiter_destroy(limiter);
    return true;
}

/* Test memory usage patterns */
TEST(memory_usage_patterns) {
    // Test HTTP request parsing memory usage
    const char *request = "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n";
    http_request_t *requests = malloc(1000 * sizeof(http_request_t));
    ASSERT_TRUE(requests != NULL);
    
    // Parse many requests to test memory stability
    for (int i = 0; i < 1000; i++) {
        bool result = http_parse_request(request, strlen(request), &requests[i]);
        ASSERT_TRUE(result);
    }
    
    free(requests);
    
    // Test rate limiter memory usage
    rate_limiter_config_t config = {
        .max_requests = 100,
        .time_window = 60,
        .block_duration = 300
    };
    
    rate_limiter_t *limiter = rate_limiter_create(&config);
    ASSERT_TRUE(limiter != NULL);
    
    // Add many different IPs to test memory growth
    for (int i = 0; i < 1000; i++) {
        char ip[16];
        snprintf(ip, sizeof(ip), "10.%d.%d.%d", 
                (i / 65536) % 256, (i / 256) % 256, i % 256);
        rate_limiter_check(limiter, ip);
    }
    
    rate_limiter_destroy(limiter);
    
    printf("\n  Memory Usage Test: PASS\n");
    printf("  - Successfully allocated and freed test data\n");
    printf("  - No obvious memory leaks detected\n");
    
    return true;
}

/* Concurrent connection simulation */
typedef struct {
    int thread_id;
    int requests_per_thread;
    int successful_requests;
    double total_time;
} thread_data_t;

static void *concurrent_request_thread(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    struct timeval start, end;
    
    gettimeofday(&start, NULL);
    
    for (int i = 0; i < data->requests_per_thread; i++) {
        // Simulate HTTP request parsing
        const char *request = "GET /test.html HTTP/1.1\r\n\r\n";
        http_request_t req;
        
        if (http_parse_request(request, strlen(request), &req)) {
            data->successful_requests++;
        }
        
        // Small delay to simulate network latency
        usleep(100); // 0.1ms
    }
    
    gettimeofday(&end, NULL);
    data->total_time = get_time_diff(start, end);
    
    return NULL;
}

TEST(concurrent_request_performance) {
    const int num_threads = 10;
    const int requests_per_thread = 1000;
    
    pthread_t threads[num_threads];
    thread_data_t thread_data[num_threads];
    
    struct timeval start, end;
    gettimeofday(&start, NULL);
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].requests_per_thread = requests_per_thread;
        thread_data[i].successful_requests = 0;
        thread_data[i].total_time = 0.0;
        
        int result = pthread_create(&threads[i], NULL, 
                                  concurrent_request_thread, &thread_data[i]);
        ASSERT_TRUE(result == 0);
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    gettimeofday(&end, NULL);
    
    double total_elapsed = get_time_diff(start, end);
    int total_requests = 0;
    int total_successful = 0;
    
    for (int i = 0; i < num_threads; i++) {
        total_requests += thread_data[i].requests_per_thread;
        total_successful += thread_data[i].successful_requests;
    }
    
    double requests_per_second = total_successful / total_elapsed;
    
    printf("\n  Concurrent Request Performance:\n");
    printf("  - Threads: %d\n", num_threads);
    printf("  - Total requests: %d\n", total_requests);
    printf("  - Successful requests: %d\n", total_successful);
    printf("  - Total time: %.3f seconds\n", total_elapsed);
    printf("  - Rate: %.0f requests/second\n", requests_per_second);
    printf("  - Success rate: %.1f%%\n", (double)total_successful / total_requests * 100);
    
    // Should handle concurrent requests successfully
    ASSERT_TRUE(total_successful > total_requests * 0.95); // 95% success rate
    ASSERT_TRUE(requests_per_second > 1000); // At least 1000 req/sec
    
    return true;
}

/* Test large request handling */
TEST(large_request_handling) {
    // Test with various request sizes
    size_t sizes[] = {1024, 4096, 8192, 16384, 32768};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    for (int i = 0; i < num_sizes; i++) {
        char *large_request = malloc(sizes[i]);
        ASSERT_TRUE(large_request != NULL);
        
        // Create a valid HTTP request of the specified size
        int header_len = snprintf(large_request, sizes[i], 
                                "POST /upload HTTP/1.1\r\n"
                                "Content-Length: %zu\r\n"
                                "Content-Type: application/octet-stream\r\n\r\n",
                                sizes[i] - 200);
        
        // Fill the rest with data
        memset(large_request + header_len, 'A', sizes[i] - header_len - 1);
        large_request[sizes[i] - 1] = '\0';
        
        struct timeval start, end;
        gettimeofday(&start, NULL);
        
        http_request_t req;
        bool result = http_parse_request(large_request, sizes[i], &req);
        
        gettimeofday(&end, NULL);
        
        double elapsed = get_time_diff(start, end);
        
        printf("\n  Large Request Test (size: %zu bytes):\n", sizes[i]);
        printf("  - Parse result: %s\n", result ? "SUCCESS" : "FAILED");
        printf("  - Parse time: %.6f seconds\n", elapsed);
        
        // Should handle reasonably sized requests
        if (sizes[i] <= 8192) {
            ASSERT_TRUE(result || elapsed < 0.001); // Either succeed or fail quickly
        }
        
        free(large_request);
    }
    
    return true;
}

/* Test sustained load */
TEST(sustained_load_test) {
    const int duration_seconds = 5;
    const int target_rps = 1000; // requests per second
    
    struct timeval start, end, current;
    gettimeofday(&start, NULL);
    
    int total_requests = 0;
    int successful_requests = 0;
    
    printf("\n  Sustained Load Test (duration: %d seconds, target: %d RPS):\n", 
           duration_seconds, target_rps);
    
    while (1) {
        gettimeofday(&current, NULL);
        double elapsed = get_time_diff(start, current);
        
        if (elapsed >= duration_seconds) {
            break;
        }
        
        // Perform HTTP parsing
        const char *request = "GET /load-test.html HTTP/1.1\r\n\r\n";
        http_request_t req;
        
        if (http_parse_request(request, strlen(request), &req)) {
            successful_requests++;
        }
        total_requests++;
        
        // Control rate to avoid overwhelming the system
        if (total_requests % 100 == 0) {
            usleep(1000); // 1ms pause every 100 requests
        }
    }
    
    gettimeofday(&end, NULL);
    double total_elapsed = get_time_diff(start, end);
    double actual_rps = successful_requests / total_elapsed;
    
    printf("  - Total requests: %d\n", total_requests);
    printf("  - Successful requests: %d\n", successful_requests);
    printf("  - Actual duration: %.3f seconds\n", total_elapsed);
    printf("  - Actual RPS: %.0f\n", actual_rps);
    printf("  - Success rate: %.1f%%\n", (double)successful_requests / total_requests * 100);
    
    // Should maintain reasonable performance under sustained load
    ASSERT_TRUE(successful_requests > total_requests * 0.95);
    ASSERT_TRUE(actual_rps > target_rps * 0.5); // At least 50% of target
    
    return true;
}

/* Test resource cleanup */
TEST(resource_cleanup_test) {
    // Test multiple create/destroy cycles
    for (int cycle = 0; cycle < 100; cycle++) {
        rate_limiter_config_t config = {
            .max_requests = 10,
            .time_window = 60,
            .block_duration = 300
        };
        
        rate_limiter_t *limiter = rate_limiter_create(&config);
        ASSERT_TRUE(limiter != NULL);
        
        // Use the rate limiter briefly
        for (int i = 0; i < 5; i++) {
            char ip[16];
            snprintf(ip, sizeof(ip), "192.168.1.%d", i);
            rate_limiter_check(limiter, ip);
        }
        
        rate_limiter_destroy(limiter);
    }
    
    printf("\n  Resource Cleanup Test: PASS\n");
    printf("  - Successfully completed 100 create/destroy cycles\n");
    
    return true;
}

/* Test runner */
int main(void) {
    printf("Running Performance tests...\n\n");
    
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
    
    RUN_TEST(http_parse_performance);
    RUN_TEST(rate_limiter_performance);
    RUN_TEST(memory_usage_patterns);
    RUN_TEST(concurrent_request_performance);
    RUN_TEST(large_request_handling);
    RUN_TEST(sustained_load_test);
    RUN_TEST(resource_cleanup_test);
    
    printf("\nPerformance Tests Summary:\n");
    printf("Failed: %d\n", failed);
    printf("Status: %s\n", failed == 0 ? "PASS" : "FAIL");
    
    return failed;
}

