#!/bin/bash

# Comprehensive Test Runner for Zircon Web Server
# This script runs all unit tests, integration tests, and performance tests

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
TEST_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$TEST_DIR")"
BUILD_DIR="$PROJECT_ROOT/obj"
LOG_DIR="/tmp/zircon_test_logs"

# Test statistics
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_TESTS=0

# Create log directory
mkdir -p "$LOG_DIR"

# Function to print colored output
print_status() {
    local status="$1"
    local message="$2"
    case "$status" in
        "INFO")  echo -e "${BLUE}[INFO]${NC} $message" ;;
        "PASS")  echo -e "${GREEN}[PASS]${NC} $message" ;;
        "FAIL")  echo -e "${RED}[FAIL]${NC} $message" ;;
        "WARN")  echo -e "${YELLOW}[WARN]${NC} $message" ;;
        "SKIP")  echo -e "${YELLOW}[SKIP]${NC} $message" ;;
    esac
}

# Function to print section headers
print_header() {
    echo
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE} $1${NC}"
    echo -e "${BLUE}================================${NC}"
    echo
}

# Function to run a test and capture results
run_test() {
    local test_name="$1"
    local test_command="$2"
    local log_file="$LOG_DIR/${test_name}.log"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    echo -n "Running $test_name... "
    
    if eval "$test_command" > "$log_file" 2>&1; then
        print_status "PASS" "$test_name"
        PASSED_TESTS=$((PASSED_TESTS + 1))
        return 0
    else
        print_status "FAIL" "$test_name"
        echo "  Log: $log_file"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return 1
    fi
}

# Function to check if a binary exists
check_binary() {
    local binary="$1"
    if [ ! -f "$binary" ]; then
        print_status "SKIP" "Binary $binary not found"
        SKIPPED_TESTS=$((SKIPPED_TESTS + 1))
        return 1
    fi
    return 0
}

# Function to build test binaries
build_tests() {
    print_header "Building Test Binaries"
    
    cd "$PROJECT_ROOT"
    
    # Build main project first
    print_status "INFO" "Building main project..."
    if ! make clean > "$LOG_DIR/build_main.log" 2>&1; then
        print_status "FAIL" "Failed to clean main project"
        return 1
    fi
    
    if ! make > "$LOG_DIR/build_main.log" 2>&1; then
        print_status "FAIL" "Failed to build main project"
        return 1
    fi
    
    print_status "PASS" "Main project built successfully"
    
    # Build test binaries
    local test_sources=(
        "test_http"
        "test_request_validator"
        "test_security_comprehensive"
        "test_rate_limiter"
        "test_logger"
        "test_performance"
    )
    
    for test_src in "${test_sources[@]}"; do
        print_status "INFO" "Building $test_src..."
        
        local compile_cmd="gcc -std=c99 -Wall -Wextra -g -O2 \
            -I$PROJECT_ROOT/include \
            -I$PROJECT_ROOT/src \
            $TEST_DIR/${test_src}.c \
            $PROJECT_ROOT/src/http.c \
            $PROJECT_ROOT/src/request_validator.c \
            $PROJECT_ROOT/src/security.c \
            $PROJECT_ROOT/src/security_config.c \
            $PROJECT_ROOT/src/rate_limiter.c \
            $PROJECT_ROOT/src/logger.c \
            -lpthread \
            -o $BUILD_DIR/${test_src}"
        
        if eval "$compile_cmd" > "$LOG_DIR/build_${test_src}.log" 2>&1; then
            print_status "PASS" "Built $test_src"
        else
            print_status "FAIL" "Failed to build $test_src"
            echo "  Build log: $LOG_DIR/build_${test_src}.log"
        fi
    done
}

# Function to run unit tests
run_unit_tests() {
    print_header "Unit Tests"
    
    local unit_tests=(
        "test_http"
        "test_request_validator"
        "test_rate_limiter"
        "test_logger"
    )
    
    for test in "${unit_tests[@]}"; do
        local binary="$BUILD_DIR/$test"
        if check_binary "$binary"; then
            run_test "$test" "$binary"
        fi
    done
}

# Function to run security tests
run_security_tests() {
    print_header "Security Tests"
    
    # Comprehensive security tests
    local binary="$BUILD_DIR/test_security_comprehensive"
    if check_binary "$binary"; then
        run_test "security_comprehensive" "$binary"
    fi
    
    # Original security shell script
    if [ -f "$TEST_DIR/test_security.sh" ]; then
        run_test "security_shell_script" "cd $PROJECT_ROOT && bash $TEST_DIR/test_security.sh"
    fi
    
    # Enhanced security script
    if [ -f "$PROJECT_ROOT/test_security.sh" ]; then
        run_test "security_enhanced" "cd $PROJECT_ROOT && bash $PROJECT_ROOT/test_security.sh"
    fi
}

# Function to run performance tests
run_performance_tests() {
    print_header "Performance Tests"
    
    local binary="$BUILD_DIR/test_performance"
    if check_binary "$binary"; then
        run_test "performance_suite" "$binary"
    fi
}

# Function to run integration tests
run_integration_tests() {
    print_header "Integration Tests"
    
    # Original test suite
    local binary="$BUILD_DIR/test_suite"
    if [ -f "$TEST_DIR/test_suite.c" ]; then
        local compile_cmd="gcc -std=c99 -Wall -Wextra -g \
            -I$PROJECT_ROOT/include \
            $TEST_DIR/test_suite.c \
            $PROJECT_ROOT/src/*.c \
            -lpthread \
            -o $binary"
        
        if eval "$compile_cmd" > "$LOG_DIR/build_test_suite.log" 2>&1; then
            if check_binary "$binary"; then
                run_test "integration_test_suite" "$binary"
            fi
        else
            print_status "FAIL" "Failed to build integration test suite"
        fi
    fi
    
    # Shell script integration tests
    local shell_tests=(
        "test-improved.sh"
        "test-sequence-fixed.sh"
        "run_tests.sh"
    )
    
    for script in "${shell_tests[@]}"; do
        local script_path="$PROJECT_ROOT/$script"
        if [ -f "$script_path" ]; then
            run_test "integration_${script%.*}" "cd $PROJECT_ROOT && timeout 60 bash $script_path"
        else
            script_path="$TEST_DIR/$script"
            if [ -f "$script_path" ]; then
                run_test "integration_${script%.*}" "cd $PROJECT_ROOT && timeout 60 bash $script_path"
            fi
        fi
    done
}

# Function to run memory tests with valgrind
run_memory_tests() {
    print_header "Memory Tests"
    
    if ! command -v valgrind &> /dev/null; then
        print_status "SKIP" "Valgrind not available - skipping memory tests"
        return
    fi
    
    local memory_tests=(
        "test_http"
        "test_rate_limiter"
        "test_logger"
    )
    
    for test in "${memory_tests[@]}"; do
        local binary="$BUILD_DIR/$test"
        if check_binary "$binary"; then
            local valgrind_cmd="valgrind --leak-check=full --error-exitcode=1 --quiet $binary"
            run_test "memory_${test}" "$valgrind_cmd"
        fi
    done
}

# Function to generate test report
generate_report() {
    print_header "Test Report"
    
    local report_file="$LOG_DIR/test_report.txt"
    local html_report="$LOG_DIR/test_report.html"
    
    # Text report
    {
        echo "Zircon Web Server Test Report"
        echo "Generated: $(date)"
        echo "=============================="
        echo
        echo "Test Summary:"
        echo "  Total Tests: $TOTAL_TESTS"
        echo "  Passed: $PASSED_TESTS"
        echo "  Failed: $FAILED_TESTS"
        echo "  Skipped: $SKIPPED_TESTS"
        echo
        if [ $FAILED_TESTS -eq 0 ]; then
            echo "Overall Status: PASS"
        else
            echo "Overall Status: FAIL"
        fi
        echo
        echo "Detailed Logs:"
        for log in "$LOG_DIR"/*.log; do
            if [ -f "$log" ]; then
                echo "  - $(basename "$log")"
            fi
        done
    } > "$report_file"
    
    # HTML report
    {
        echo "<html><head><title>Zircon Test Report</title></head><body>"
        echo "<h1>Zircon Web Server Test Report</h1>"
        echo "<p>Generated: $(date)</p>"
        echo "<h2>Summary</h2>"
        echo "<ul>"
        echo "<li>Total Tests: $TOTAL_TESTS</li>"
        echo "<li>Passed: <span style='color:green'>$PASSED_TESTS</span></li>"
        echo "<li>Failed: <span style='color:red'>$FAILED_TESTS</span></li>"
        echo "<li>Skipped: <span style='color:orange'>$SKIPPED_TESTS</span></li>"
        echo "</ul>"
        if [ $FAILED_TESTS -eq 0 ]; then
            echo "<h2 style='color:green'>Overall Status: PASS</h2>"
        else
            echo "<h2 style='color:red'>Overall Status: FAIL</h2>"
        fi
        echo "</body></html>"
    } > "$html_report"
    
    print_status "INFO" "Test report generated: $report_file"
    print_status "INFO" "HTML report generated: $html_report"
}

# Function to cleanup
cleanup() {
    print_header "Cleanup"
    
    # Stop any running test servers
    pkill -f "zircon" 2>/dev/null || true
    
    # Clean up temporary files
    rm -f /tmp/test_*.log
    rm -f /tmp/test_zircon*.log
    
    print_status "INFO" "Cleanup completed"
}

# Main execution
main() {
    print_header "Zircon Web Server Comprehensive Test Suite"
    
    print_status "INFO" "Starting test execution at $(date)"
    print_status "INFO" "Project root: $PROJECT_ROOT"
    print_status "INFO" "Test directory: $TEST_DIR"
    print_status "INFO" "Log directory: $LOG_DIR"
    
    # Build tests
    build_tests
    
    # Run test suites
    run_unit_tests
    run_security_tests
    run_performance_tests
    run_integration_tests
    run_memory_tests
    
    # Generate report
    generate_report
    
    # Cleanup
    cleanup
    
    # Final summary
    print_header "Final Summary"
    echo -e "Total Tests: ${BLUE}$TOTAL_TESTS${NC}"
    echo -e "Passed: ${GREEN}$PASSED_TESTS${NC}"
    echo -e "Failed: ${RED}$FAILED_TESTS${NC}"
    echo -e "Skipped: ${YELLOW}$SKIPPED_TESTS${NC}"
    echo
    
    if [ $FAILED_TESTS -eq 0 ]; then
        echo -e "${GREEN}🎉 All tests passed!${NC}"
        exit 0
    else
        echo -e "${RED}❌ Some tests failed. Check logs in $LOG_DIR${NC}"
        exit 1
    fi
}

# Handle script arguments
case "${1:-}" in
    "unit")
        build_tests
        run_unit_tests
        ;;
    "security")
        build_tests
        run_security_tests
        ;;
    "performance")
        build_tests
        run_performance_tests
        ;;
    "integration")
        build_tests
        run_integration_tests
        ;;
    "memory")
        build_tests
        run_memory_tests
        ;;
    "build")
        build_tests
        ;;
    "clean")
        cleanup
        ;;
    *)
        main
        ;;
esac

