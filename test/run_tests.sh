#!/bin/sh

# Colors for output (using tput for better portability)
if [ -t 1 ]; then
    GREEN=$(tput setaf 2)
    RED=$(tput setaf 1)
    YELLOW=$(tput setaf 3)
    NC=$(tput sgr0)
else
    GREEN=""
    RED=""
    YELLOW=""
    NC=""
fi

# Test counters
TOTAL=0
PASSED=0
FAILED=0

# Server configuration
SERVER_HOST=${SERVER_HOST:-localhost}
SERVER_PORT=${SERVER_PORT:-8000}
SERVER_BINARY=${SERVER_BINARY:-misewe}
BASE_URL="http://${SERVER_HOST}:${SERVER_PORT}"

print_header() {
    printf "\n%s=== %s ===%s\n\n" "${YELLOW}" "$1" "${NC}"
}

run_test() {
    name="$1"
    cmd="$2"
    expected="$3"
    
    printf "Testing %s... " "$name"
    TOTAL=$((TOTAL + 1))
    
    output=$(eval "$cmd" 2>&1)
    result=$?
    
    if [ $result -eq "$expected" ]; then
        printf "%sPASSED%s\n" "${GREEN}" "${NC}"
        PASSED=$((PASSED + 1))
    else
        printf "%sFAILED%s\n" "${RED}" "${NC}"
        printf "Expected exit code: %d, got: %d\n" "$expected" "$result"
        printf "Command output:\n%s\n" "$output"
        FAILED=$((FAILED + 1))
    fi
}

# Check if server is running
check_server() {
    if ! curl -s "$BASE_URL" >/dev/null 2>&1; then
        printf "%sError: Server is not running at %s%s\n" "${RED}" "$BASE_URL" "${NC}"
        printf "Please start the server first:\n"
        printf "  ./bin/%s\n" "${SERVER_BINARY}"
        exit 1
    fi
}

# Basic functionality tests
test_basic_functionality() {
    print_header "Basic Functionality Tests"

    run_test "basic HTML access" \
        "curl -s $BASE_URL/index.html" 0

    run_test "CSS file access" \
        "curl -s $BASE_URL/style.css" 0

    run_test "404 handling" \
        "curl -s -f $BASE_URL/nonexistent.html" 22
}

# Security tests
test_security() {
    print_header "Security Tests"

    run_test "path traversal prevention" \
        "curl -s -f $BASE_URL/../etc/passwd" 22

    run_test "file type restrictions" \
        "curl -s -f $BASE_URL/test.php" 22

    run_test "XSS protection headers" \
        "curl -s -I $BASE_URL/index.html | grep -q 'X-XSS-Protection'" 0
}

# Rate limiting test
test_rate_limiting() {
    print_header "Rate Limiting Test"
    printf "Testing rate limiting... "
    TOTAL=$((TOTAL + 1))

    count=0
    blocked=0
    i=0
    while [ $i -lt 100 ] && [ $blocked -eq 0 ]; do
        response=$(curl -s -w "%{http_code}" -o /dev/null "$BASE_URL/")
        if [ "$response" = "429" ]; then
            blocked=1
        elif [ "$response" = "200" ]; then
            count=$((count + 1))
        else
            printf "%sFAILED%s (Unexpected response code: %s)\n" "${RED}" "${NC}" "$response"
            FAILED=$((FAILED + 1))
            return 1
        fi
        i=$((i + 1))
    done

    if [ $blocked -eq 1 ]; then
        printf "%sPASSED%s (Blocked after %d requests)\n" "${GREEN}" "${NC}" "$count"
        PASSED=$((PASSED + 1))
    else
        printf "%sFAILED%s (No rate limiting after %d requests)\n" "${RED}" "${NC}" "$count"
        FAILED=$((FAILED + 1))
    fi
}

# Security headers test
test_security_headers() {
    print_header "Security Headers Test"

    headers=$(curl -s -I "$BASE_URL/index.html")
    for header in "X-Frame-Options" "X-Content-Type-Options" "X-XSS-Protection"; do
        printf "Testing %s... " "$header"
        TOTAL=$((TOTAL + 1))
        if printf "%s" "$headers" | grep -q "$header"; then
            printf "%sPASSED%s\n" "${GREEN}" "${NC}"
            PASSED=$((PASSED + 1))
        else
            printf "%sFAILED%s\n" "${RED}" "${NC}"
            FAILED=$((FAILED + 1))
        fi
    done
}

# Main test execution
main() {
    print_header "Zircon Test Suite"
    check_server

    test_basic_functionality
    test_security
    test_rate_limiting
    test_security_headers

    # Print summary
    print_header "Test Summary"
    printf "Total tests: %s%d%s\n" "${YELLOW}" "$TOTAL" "${NC}"
    printf "Passed: %s%d%s\n" "${GREEN}" "$PASSED" "${NC}"
    printf "Failed: %s%d%s\n" "${RED}" "$FAILED" "${NC}"

    # Exit with status based on test results
    if [ $FAILED -eq 0 ]; then
        printf "\n%sAll tests passed!%s\n" "${GREEN}" "${NC}"
        exit 0
    else
        printf "\n%sSome tests failed.%s\n" "${RED}" "${NC}"
        printf "Check the test output above for details.\n"
        exit 1
    fi
}

main 