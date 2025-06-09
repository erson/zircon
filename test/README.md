# Zircon Web Server Test Suite

This directory contains a comprehensive test suite for the Zircon web server, covering unit tests, integration tests, security tests, and performance tests.

## 📋 Test Overview

### Test Categories

1. **Unit Tests** - Test individual functions and modules
2. **Security Tests** - Comprehensive security vulnerability testing
3. **Performance Tests** - Load testing and performance benchmarks
4. **Integration Tests** - End-to-end functionality testing
5. **Memory Tests** - Memory leak detection and resource management

### Test Files

#### Unit Test Files
- `test_http.c` - HTTP request parsing and response generation
- `test_request_validator.c` - Request validation and security checks
- `test_rate_limiter.c` - Rate limiting functionality
- `test_logger.c` - Logging system tests
- `test_config.c` - Configuration loading and validation

#### Security Test Files
- `test_security_comprehensive.c` - Comprehensive security testing with attack payloads
- `test_security.sh` - Shell-based security tests
- `security_test.sh` - Additional security validation

#### Performance Test Files
- `test_performance.c` - Performance benchmarks and load testing

#### Integration Test Files
- `test_suite.c` - Original integration test suite
- `run_tests.sh` - Shell-based integration tests

#### Test Automation
- `run_all_tests.sh` - Master test runner script
- `TESTING_GUIDE.md` - Detailed testing procedures

## 🚀 Quick Start

### Running All Tests

```bash
# Run the complete test suite
./test/run_all_tests.sh

# Run specific test categories
./test/run_all_tests.sh unit        # Unit tests only
./test/run_all_tests.sh security    # Security tests only
./test/run_all_tests.sh performance # Performance tests only
./test/run_all_tests.sh integration # Integration tests only
./test/run_all_tests.sh memory      # Memory tests with valgrind
```

### Running Individual Tests

```bash
# Build and run a specific test
cd /path/to/zircon
make
gcc -I include test/test_http.c src/http.c -o test_http
./test_http

# Run with valgrind for memory checking
valgrind --leak-check=full ./test_http
```

## 🔧 Building Tests

### Prerequisites

- GCC compiler with C99 support
- Make build system
- pthread library
- valgrind (optional, for memory tests)

### Build Process

The test runner automatically builds all test binaries, but you can build manually:

```bash
# Build main project first
make clean && make

# Build individual test
gcc -std=c99 -Wall -Wextra -g -O2 \
    -I include \
    test/test_http.c \
    src/http.c \
    src/request_validator.c \
    src/security.c \
    src/security_config.c \
    src/rate_limiter.c \
    src/logger.c \
    -lpthread \
    -o obj/test_http
```

## 📊 Test Results

### Understanding Test Output

Tests provide detailed output including:
- Individual test results (PASS/FAIL)
- Performance metrics (requests/second, response times)
- Memory usage information
- Security vulnerability detection results

### Log Files

Test logs are stored in `/tmp/zircon_test_logs/`:
- `test_report.txt` - Summary report
- `test_report.html` - HTML formatted report
- Individual test logs for debugging

### Exit Codes

- `0` - All tests passed
- `1` - One or more tests failed
- `2` - Build failure
- `3` - Test setup failure

## 🛡️ Security Testing

### Attack Payload Coverage

The security tests include comprehensive attack vectors:

#### XSS (Cross-Site Scripting)
- Script injection attempts
- Event handler exploitation
- URL-based XSS
- Encoded payload variants
- DOM manipulation attempts

#### SQL Injection
- Union-based attacks
- Boolean-based blind injection
- Time-based blind injection
- Error-based injection
- Stacked queries

#### Path Traversal
- Directory traversal attempts
- Encoded path traversal
- Double encoding
- Null byte injection
- Windows and Unix path variants

### Security Test Examples

```bash
# Run comprehensive security tests
./test/run_all_tests.sh security

# Run specific security test
./obj/test_security_comprehensive

# Run shell-based security tests
bash test_security.sh
```

## ⚡ Performance Testing

### Performance Metrics

The performance tests measure:
- HTTP parsing speed (requests/second)
- Rate limiter performance (checks/second)
- Memory usage patterns
- Concurrent request handling
- Sustained load capacity

### Performance Benchmarks

Expected performance baselines:
- HTTP parsing: >10,000 requests/second
- Rate limiting: >5,000 checks/second
- Concurrent connections: >95% success rate
- Memory usage: No leaks detected

### Running Performance Tests

```bash
# Run all performance tests
./test/run_all_tests.sh performance

# Run specific performance test
./obj/test_performance
```

## 🧪 Unit Testing

### Test Structure

Each unit test file follows a consistent pattern:

```c
#include <stdio.h>
#include <assert.h>
#include "../include/module.h"

#define TEST(name) static bool test_##name(void)
#define ASSERT_TRUE(expr) // Assertion macro

TEST(function_basic_functionality) {
    // Test implementation
    ASSERT_TRUE(condition);
    return true;
}

int main(void) {
    // Test runner
    RUN_TEST(function_basic_functionality);
    return failed_count;
}
```

### Adding New Unit Tests

1. Create test file: `test/test_new_module.c`
2. Include necessary headers
3. Implement test functions using `TEST()` macro
4. Add assertions using `ASSERT_TRUE()`, `ASSERT_FALSE()`, etc.
5. Update `run_all_tests.sh` to include the new test
6. Build and run tests

## 🔍 Memory Testing

### Valgrind Integration

Memory tests use valgrind to detect:
- Memory leaks
- Buffer overflows
- Use after free
- Double free errors
- Uninitialized memory access

### Running Memory Tests

```bash
# Run memory tests (requires valgrind)
./test/run_all_tests.sh memory

# Manual valgrind execution
valgrind --leak-check=full --error-exitcode=1 ./obj/test_http
```

## 🔄 Integration Testing

### Integration Test Coverage

Integration tests verify:
- Complete HTTP request/response cycle
- Security feature integration
- Rate limiting in real scenarios
- Server startup/shutdown
- Configuration loading
- Error handling

### Shell Script Tests

Several shell scripts provide integration testing:
- `test-improved.sh` - Enhanced HTTP functionality tests
- `test-sequence-fixed.sh` - Sequential test execution
- `run_tests.sh` - Basic integration tests

## 📈 Continuous Integration

### CI/CD Integration

The test suite is designed for CI/CD integration:

```yaml
# Example GitHub Actions workflow
- name: Run Tests
  run: |
    make clean && make
    ./test/run_all_tests.sh
    
- name: Upload Test Results
  uses: actions/upload-artifact@v2
  with:
    name: test-results
    path: /tmp/zircon_test_logs/
```

### Test Automation Features

- Automatic test discovery
- Parallel test execution
- Detailed reporting
- Exit code handling
- Log aggregation

## 🐛 Debugging Tests

### Common Issues

1. **Build Failures**
   - Check include paths
   - Verify all source files are present
   - Ensure proper linking flags

2. **Test Failures**
   - Check test logs in `/tmp/zircon_test_logs/`
   - Run individual tests for detailed output
   - Use valgrind for memory-related issues

3. **Performance Issues**
   - Verify system resources
   - Check for background processes
   - Run tests on dedicated test environment

### Debug Commands

```bash
# Build with debug symbols
gcc -g -DDEBUG test/test_http.c src/http.c -o test_http

# Run with GDB
gdb ./test_http

# Run with detailed valgrind output
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./test_http
```

## 📝 Contributing

### Adding New Tests

1. Follow the existing test structure
2. Include comprehensive edge case testing
3. Add performance considerations
4. Update documentation
5. Ensure tests are deterministic

### Test Guidelines

- Tests should be independent and isolated
- Use descriptive test names
- Include both positive and negative test cases
- Test edge cases and error conditions
- Provide clear failure messages

### Code Coverage

Aim for high code coverage:
- Function coverage: >90%
- Line coverage: >80%
- Branch coverage: >75%

## 📚 Additional Resources

- [TESTING_GUIDE.md](TESTING_GUIDE.md) - Detailed testing procedures
- [Security Testing Best Practices](https://owasp.org/www-project-web-security-testing-guide/)
- [C Unit Testing Frameworks](https://en.wikipedia.org/wiki/List_of_unit_testing_frameworks#C)
- [Valgrind Documentation](https://valgrind.org/docs/manual/)

## 🆘 Support

For questions or issues with the test suite:

1. Check the test logs for detailed error information
2. Review this documentation and the testing guide
3. Run individual tests to isolate issues
4. Use debugging tools (gdb, valgrind) for complex problems

---

**Note**: This test suite is designed to be comprehensive and may take several minutes to complete. For faster development cycles, use the category-specific test runners.

