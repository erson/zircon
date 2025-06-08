# Zircon Web Server Testing Guide

This comprehensive guide covers all aspects of testing the Zircon web server, from basic unit tests to advanced security and performance testing.

## 📖 Table of Contents

1. [Testing Philosophy](#testing-philosophy)
2. [Test Environment Setup](#test-environment-setup)
3. [Unit Testing](#unit-testing)
4. [Security Testing](#security-testing)
5. [Performance Testing](#performance-testing)
6. [Integration Testing](#integration-testing)
7. [Memory Testing](#memory-testing)
8. [Test Automation](#test-automation)
9. [Debugging and Troubleshooting](#debugging-and-troubleshooting)
10. [Best Practices](#best-practices)

## 🎯 Testing Philosophy

### Testing Pyramid

The Zircon test suite follows the testing pyramid approach:

```
    /\
   /  \     E2E Tests (Integration)
  /____\    
 /      \   Integration Tests
/________\   
|        |  Unit Tests (Foundation)
|________|  
```

- **Unit Tests (70%)**: Fast, isolated tests for individual functions
- **Integration Tests (20%)**: Component interaction testing
- **End-to-End Tests (10%)**: Full system testing

### Security-First Testing

Given Zircon's security focus, our testing emphasizes:
- Comprehensive attack vector coverage
- Input validation testing
- Boundary condition testing
- Error handling verification
- Resource exhaustion testing

## 🛠️ Test Environment Setup

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install build-essential gcc make valgrind curl

# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install gcc make valgrind curl

# macOS
xcode-select --install
brew install valgrind
```

### Environment Variables

```bash
export ZIRCON_TEST_DIR="/tmp/zircon_tests"
export ZIRCON_LOG_LEVEL="DEBUG"
export ZIRCON_TEST_TIMEOUT="60"
```

### Directory Structure

```
test/
├── README.md                    # Test overview
├── TESTING_GUIDE.md            # This guide
├── run_all_tests.sh            # Master test runner
├── test_http.c                 # HTTP module tests
├── test_request_validator.c    # Request validation tests
├── test_security_comprehensive.c # Security tests
├── test_rate_limiter.c         # Rate limiting tests
├── test_logger.c               # Logging tests
├── test_performance.c          # Performance tests
├── test_config.c               # Configuration tests
├── test_suite.c                # Integration tests
└── config_samples/             # Test configuration files
```

## 🧪 Unit Testing

### Writing Unit Tests

#### Basic Test Structure

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../include/module.h"

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

TEST(function_basic_functionality) {
    // Arrange
    input_type input = create_test_input();
    
    // Act
    result_type result = function_under_test(input);
    
    // Assert
    ASSERT_TRUE(result.is_valid);
    ASSERT_EQ(result.value, expected_value);
    
    // Cleanup
    cleanup_test_input(input);
    return true;
}
```

#### Test Categories

1. **Happy Path Tests**: Normal operation scenarios
2. **Edge Case Tests**: Boundary conditions
3. **Error Condition Tests**: Invalid inputs and error handling
4. **Resource Tests**: Memory allocation and cleanup

#### Example: HTTP Parser Tests

```c
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

TEST(http_parse_request_invalid_format) {
    const char *request = "INVALID REQUEST FORMAT";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    ASSERT_FALSE(result);
    return true;
}
```

### Running Unit Tests

```bash
# Build and run all unit tests
./test/run_all_tests.sh unit

# Build and run specific test
gcc -I include test/test_http.c src/http.c -o test_http
./test_http

# Run with debugging
gcc -g -DDEBUG -I include test/test_http.c src/http.c -o test_http
gdb ./test_http
```

## 🛡️ Security Testing

### Attack Vector Coverage

#### XSS (Cross-Site Scripting) Testing

```c
static const char *xss_payloads[] = {
    "<script>alert('XSS')</script>",
    "<img src=x onerror=alert('XSS')>",
    "<svg onload=alert('XSS')>",
    "javascript:alert('XSS')",
    "<iframe src=javascript:alert('XSS')>",
    // ... more payloads
    NULL
};

TEST(security_xss_detection) {
    security_ctx_t *ctx = security_create(&config);
    
    for (int i = 0; xss_payloads[i] != NULL; i++) {
        bool result = security_check_request(ctx, "127.0.0.1", "GET", "/", 
                                           xss_payloads[i], xss_payloads[i], 
                                           strlen(xss_payloads[i]));
        ASSERT_FALSE(result); // Should block XSS attempts
    }
    
    security_destroy(ctx);
    return true;
}
```

#### SQL Injection Testing

```c
static const char *sql_injection_payloads[] = {
    "' OR '1'='1",
    "' OR 1=1--",
    "'; DROP TABLE users--",
    "' UNION SELECT 1,2,3--",
    // ... more payloads
    NULL
};

TEST(security_sql_injection_detection) {
    security_ctx_t *ctx = security_create(&config);
    
    for (int i = 0; sql_injection_payloads[i] != NULL; i++) {
        bool result = security_check_request(ctx, "127.0.0.1", "GET", "/", 
                                           sql_injection_payloads[i], 
                                           sql_injection_payloads[i], 
                                           strlen(sql_injection_payloads[i]));
        ASSERT_FALSE(result); // Should block SQL injection attempts
    }
    
    security_destroy(ctx);
    return true;
}
```

#### Path Traversal Testing

```c
static const char *path_traversal_payloads[] = {
    "../etc/passwd",
    "..\\windows\\system32\\config\\sam",
    "%2e%2e%2fetc%2fpasswd",
    "....//etc/passwd",
    // ... more payloads
    NULL
};

TEST(security_path_traversal_detection) {
    for (int i = 0; path_traversal_payloads[i] != NULL; i++) {
        bool result = is_path_safe(path_traversal_payloads[i]);
        ASSERT_FALSE(result); // Should reject traversal attempts
    }
    return true;
}
```

### Security Test Execution

```bash
# Run all security tests
./test/run_all_tests.sh security

# Run comprehensive security test
./obj/test_security_comprehensive

# Run shell-based security tests
bash test_security.sh
```

### Custom Security Payloads

Create custom payload files for specific testing:

```bash
# Create custom XSS payload file
cat > custom_xss_payloads.txt << EOF
<script>custom_payload()</script>
<img src=x onerror=custom_function()>
<svg onload=custom_attack()>
EOF

# Use in tests
while IFS= read -r payload; do
    test_xss_payload "$payload"
done < custom_xss_payloads.txt
```

## ⚡ Performance Testing

### Performance Metrics

#### HTTP Parsing Performance

```c
TEST(http_parse_performance) {
    const char *request = "GET /index.html HTTP/1.1\r\n\r\n";
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
    
    printf("HTTP Parse Rate: %.0f requests/second\n", requests_per_second);
    ASSERT_TRUE(requests_per_second > 10000); // Minimum performance threshold
    
    return true;
}
```

#### Concurrent Load Testing

```c
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
        // Simulate request processing
        const char *request = "GET /test.html HTTP/1.1\r\n\r\n";
        http_request_t req;
        
        if (http_parse_request(request, strlen(request), &req)) {
            data->successful_requests++;
        }
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
    
    // Create and run threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].requests_per_thread = requests_per_thread;
        thread_data[i].successful_requests = 0;
        
        pthread_create(&threads[i], NULL, concurrent_request_thread, &thread_data[i]);
    }
    
    // Wait for completion and analyze results
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Calculate metrics
    int total_successful = 0;
    for (int i = 0; i < num_threads; i++) {
        total_successful += thread_data[i].successful_requests;
    }
    
    double success_rate = (double)total_successful / (num_threads * requests_per_thread);
    ASSERT_TRUE(success_rate > 0.95); // 95% success rate minimum
    
    return true;
}
```

### Performance Benchmarks

Expected performance baselines:

| Component | Metric | Minimum Threshold |
|-----------|--------|-------------------|
| HTTP Parser | Requests/second | 10,000 |
| Rate Limiter | Checks/second | 5,000 |
| Security Validator | Validations/second | 8,000 |
| Logger | Log entries/second | 15,000 |

### Running Performance Tests

```bash
# Run all performance tests
./test/run_all_tests.sh performance

# Run specific performance test
./obj/test_performance

# Run with performance profiling
perf record ./obj/test_performance
perf report
```

## 🔗 Integration Testing

### Server Integration Tests

#### Full Server Lifecycle

```c
TEST(server_full_lifecycle) {
    server_config_t config = {
        .port = 8081,
        .document_root = "./www",
        .max_connections = 100
    };
    
    // Create server
    server_t *server = server_create(&config);
    ASSERT_TRUE(server != NULL);
    
    // Start server in background thread
    pthread_t server_thread;
    pthread_create(&server_thread, NULL, run_server_thread, server);
    
    // Wait for server to start
    sleep(1);
    
    // Test HTTP requests
    int sock = connect_to_server("127.0.0.1", 8081);
    ASSERT_TRUE(sock > 0);
    
    const char *request = "GET /index.html HTTP/1.1\r\n\r\n";
    send(sock, request, strlen(request), 0);
    
    char response[1024];
    int bytes_received = recv(sock, response, sizeof(response) - 1, 0);
    ASSERT_TRUE(bytes_received > 0);
    
    response[bytes_received] = '\0';
    ASSERT_TRUE(strstr(response, "HTTP/1.1 200 OK") != NULL);
    
    // Cleanup
    close(sock);
    server_stop(server);
    pthread_join(server_thread, NULL);
    server_destroy(server);
    
    return true;
}
```

#### Shell Script Integration Tests

```bash
#!/bin/bash
# test_integration.sh

SERVER_HOST="127.0.0.1"
SERVER_PORT="8080"
TEST_TIMEOUT=30

# Start server
./zircon &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Test basic functionality
test_basic_http() {
    local response=$(curl -s -w "%{http_code}" -o /dev/null "http://$SERVER_HOST:$SERVER_PORT/index.html")
    if [ "$response" = "200" ]; then
        echo "PASS: Basic HTTP test"
        return 0
    else
        echo "FAIL: Basic HTTP test (got $response)"
        return 1
    fi
}

# Test security headers
test_security_headers() {
    local headers=$(curl -s -I "http://$SERVER_HOST:$SERVER_PORT/index.html")
    
    if echo "$headers" | grep -q "X-Frame-Options: DENY"; then
        echo "PASS: X-Frame-Options header"
    else
        echo "FAIL: X-Frame-Options header missing"
        return 1
    fi
    
    if echo "$headers" | grep -q "X-Content-Type-Options: nosniff"; then
        echo "PASS: X-Content-Type-Options header"
    else
        echo "FAIL: X-Content-Type-Options header missing"
        return 1
    fi
    
    return 0
}

# Run tests
test_basic_http
test_security_headers

# Cleanup
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null
```

### Running Integration Tests

```bash
# Run all integration tests
./test/run_all_tests.sh integration

# Run specific integration test
./obj/test_suite

# Run shell-based integration tests
bash test-improved.sh
```

## 🧠 Memory Testing

### Valgrind Integration

#### Memory Leak Detection

```bash
# Run with valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test_http

# Expected output for clean code:
# ==12345== HEAP SUMMARY:
# ==12345==     in use at exit: 0 bytes in 0 blocks
# ==12345==   total heap usage: 10 allocs, 10 frees, 1,024 bytes allocated
# ==12345== 
# ==12345== All heap blocks were freed -- no leaks are possible
```

#### Buffer Overflow Detection

```bash
# Compile with AddressSanitizer
gcc -fsanitize=address -g test/test_http.c src/http.c -o test_http_asan
./test_http_asan

# Run with valgrind for additional checking
valgrind --tool=memcheck --track-origins=yes ./test_http
```

### Memory Test Implementation

```c
TEST(memory_leak_detection) {
    // Test multiple allocation/deallocation cycles
    for (int cycle = 0; cycle < 1000; cycle++) {
        // Allocate resources
        http_request_t *req = malloc(sizeof(http_request_t));
        ASSERT_TRUE(req != NULL);
        
        // Use resources
        const char *request = "GET /test.html HTTP/1.1\r\n\r\n";
        bool result = http_parse_request(request, strlen(request), req);
        ASSERT_TRUE(result);
        
        // Free resources
        free(req);
    }
    
    // Test should complete without memory leaks
    return true;
}

TEST(buffer_overflow_protection) {
    // Test with oversized inputs
    char large_request[10000];
    memset(large_request, 'A', sizeof(large_request) - 1);
    large_request[sizeof(large_request) - 1] = '\0';
    
    http_request_t req;
    
    // Should handle gracefully without buffer overflow
    bool result = http_parse_request(large_request, sizeof(large_request), &req);
    
    // Either succeed or fail gracefully, but no crash
    return true;
}
```

### Running Memory Tests

```bash
# Run memory tests with valgrind
./test/run_all_tests.sh memory

# Manual valgrind execution
valgrind --leak-check=full --error-exitcode=1 ./obj/test_http

# Run with AddressSanitizer
gcc -fsanitize=address -g test/test_http.c src/http.c -o test_http_asan
./test_http_asan
```

## 🤖 Test Automation

### Continuous Integration Setup

#### GitHub Actions Example

```yaml
name: Zircon Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install dependencies
      run: |
        sudo apt-get update
        sudo apt-get install build-essential valgrind curl
    
    - name: Build project
      run: make clean && make
    
    - name: Run unit tests
      run: ./test/run_all_tests.sh unit
    
    - name: Run security tests
      run: ./test/run_all_tests.sh security
    
    - name: Run performance tests
      run: ./test/run_all_tests.sh performance
    
    - name: Run memory tests
      run: ./test/run_all_tests.sh memory
    
    - name: Upload test results
      uses: actions/upload-artifact@v2
      if: always()
      with:
        name: test-results
        path: /tmp/zircon_test_logs/
```

#### Jenkins Pipeline Example

```groovy
pipeline {
    agent any
    
    stages {
        stage('Build') {
            steps {
                sh 'make clean && make'
            }
        }
        
        stage('Unit Tests') {
            steps {
                sh './test/run_all_tests.sh unit'
            }
        }
        
        stage('Security Tests') {
            steps {
                sh './test/run_all_tests.sh security'
            }
        }
        
        stage('Performance Tests') {
            steps {
                sh './test/run_all_tests.sh performance'
            }
        }
        
        stage('Memory Tests') {
            steps {
                sh './test/run_all_tests.sh memory'
            }
        }
    }
    
    post {
        always {
            archiveArtifacts artifacts: '/tmp/zircon_test_logs/**/*', allowEmptyArchive: true
            publishTestResults testResultsPattern: '/tmp/zircon_test_logs/test_report.xml'
        }
    }
}
```

### Test Scheduling

```bash
# Daily comprehensive test run
0 2 * * * /path/to/zircon/test/run_all_tests.sh > /var/log/zircon_daily_tests.log 2>&1

# Hourly quick tests
0 * * * * /path/to/zircon/test/run_all_tests.sh unit > /var/log/zircon_hourly_tests.log 2>&1

# Weekly performance baseline
0 3 * * 0 /path/to/zircon/test/run_all_tests.sh performance > /var/log/zircon_weekly_perf.log 2>&1
```

## 🐛 Debugging and Troubleshooting

### Common Issues and Solutions

#### Build Failures

```bash
# Issue: Missing headers
# Solution: Check include paths
gcc -I include -I src test/test_http.c src/http.c -o test_http

# Issue: Linking errors
# Solution: Add required libraries
gcc test/test_http.c src/http.c -lpthread -o test_http

# Issue: Undefined symbols
# Solution: Include all required source files
gcc test/test_http.c src/http.c src/request_validator.c src/security.c -o test_http
```

#### Test Failures

```bash
# Debug specific test
gcc -g -DDEBUG test/test_http.c src/http.c -o test_http
gdb ./test_http

# Run with verbose output
./test_http --verbose

# Check test logs
cat /tmp/zircon_test_logs/test_http.log
```

#### Memory Issues

```bash
# Detect memory leaks
valgrind --leak-check=full --show-leak-kinds=all ./test_http

# Detect buffer overflows
gcc -fsanitize=address test/test_http.c src/http.c -o test_http
./test_http

# Debug memory corruption
valgrind --tool=memcheck --track-origins=yes ./test_http
```

#### Performance Issues

```bash
# Profile performance
perf record ./test_performance
perf report

# Check system resources
top -p $(pgrep test_performance)

# Monitor memory usage
valgrind --tool=massif ./test_performance
ms_print massif.out.*
```

### Debugging Tools

#### GDB Usage

```bash
# Compile with debug symbols
gcc -g test/test_http.c src/http.c -o test_http

# Start GDB
gdb ./test_http

# GDB commands
(gdb) break main
(gdb) run
(gdb) step
(gdb) print variable_name
(gdb) backtrace
(gdb) continue
```

#### Valgrind Tools

```bash
# Memory error detection
valgrind --tool=memcheck ./test_http

# Memory profiling
valgrind --tool=massif ./test_http

# Cache profiling
valgrind --tool=cachegrind ./test_http

# Thread error detection
valgrind --tool=helgrind ./test_performance
```

## 📋 Best Practices

### Test Design Principles

1. **Independence**: Tests should not depend on each other
2. **Repeatability**: Tests should produce consistent results
3. **Fast Execution**: Unit tests should run quickly
4. **Clear Assertions**: Test failures should be easy to understand
5. **Comprehensive Coverage**: Test both success and failure paths

### Code Quality Guidelines

```c
// Good: Clear test name and purpose
TEST(http_parse_request_handles_malformed_headers) {
    const char *request = "GET /index.html HTTP/1.1\r\nMalformed Header\r\n\r\n";
    http_request_t req;
    
    bool result = http_parse_request(request, strlen(request), &req);
    
    // Should handle malformed headers gracefully
    ASSERT_FALSE(result);
    return true;
}

// Bad: Unclear test purpose
TEST(test1) {
    char *req = "GET /index.html HTTP/1.1\r\n\r\n";
    http_request_t r;
    ASSERT_TRUE(http_parse_request(req, strlen(req), &r));
    return true;
}
```

### Performance Testing Guidelines

1. **Baseline Establishment**: Record performance baselines
2. **Consistent Environment**: Use dedicated test environments
3. **Multiple Runs**: Average results across multiple runs
4. **Resource Monitoring**: Monitor CPU, memory, and I/O
5. **Regression Detection**: Alert on performance degradation

### Security Testing Guidelines

1. **Comprehensive Payloads**: Use extensive attack vector databases
2. **Real-World Scenarios**: Test actual attack patterns
3. **Boundary Testing**: Test input limits and edge cases
4. **Error Handling**: Verify secure error responses
5. **Regular Updates**: Keep attack payloads current

### Documentation Standards

1. **Test Purpose**: Document what each test validates
2. **Setup Requirements**: Document test prerequisites
3. **Expected Results**: Document expected outcomes
4. **Failure Analysis**: Document common failure causes
5. **Maintenance Notes**: Document test maintenance procedures

---

This testing guide provides comprehensive coverage of all testing aspects for the Zircon web server. Regular updates and improvements to the test suite ensure continued reliability and security of the server implementation.

