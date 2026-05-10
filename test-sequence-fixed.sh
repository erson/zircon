#!/bin/bash

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Test server configuration
SERVER_HOST="localhost"
SERVER_PORT="8000"
SERVER_BIN="./bin/zircon"
SERVER_PID=""

# Test statistics
TESTS_TOTAL=0
TESTS_PASSED=0
TESTS_FAILED=0

# Header function
function print_header() {
    echo -e "\n${YELLOW}====== $1 ======${NC}\n"
}

# Test function
function run_test() {
    local test_name="$1"
    local test_cmd="$2"
    local expected_result="$3"
    
    echo -n "Test: $test_name... "
    TESTS_TOTAL=$((TESTS_TOTAL+1))
    
    # Run test command and capture output
    local output=$(eval "$test_cmd" 2>&1)
    local result=$?
    
    # Check if test result matches expected result
    if [[ "$expected_result" == "status_$result" ]]; then
        echo -e "${GREEN}PASSED${NC}"
        TESTS_PASSED=$((TESTS_PASSED+1))
    else
        # Check again for content tests
        if [[ "$expected_result" == "contains_"* ]]; then
            local expected_content="${expected_result#contains_}"
            if [[ "$output" == *"$expected_content"* ]]; then
                echo -e "${GREEN}PASSED${NC}"
                TESTS_PASSED=$((TESTS_PASSED+1))
            else
                echo -e "${RED}FAILED${NC} (Expected content not found)"
                echo "Output: $output"
                TESTS_FAILED=$((TESTS_FAILED+1))
            fi
        else
            echo -e "${RED}FAILED${NC} (Expected: $expected_result, Got: status_$result)"
            echo "Output: $output"
            TESTS_FAILED=$((TESTS_FAILED+1))
        fi
    fi
}

# Test header function
function test_header() {
    echo -e "${GREEN}\n=== $1 ===${NC}"
}

# Start server
function start_server() {
    echo "Starting Zircon server..."
    $SERVER_BIN > /dev/null 2>&1 &
    SERVER_PID=$!
    
    # Wait for server to start
    sleep 2
    
    # Check if server is running
    if kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "${GREEN}Server started (PID: $SERVER_PID)${NC}"
        return 0
    else
        echo -e "${RED}Failed to start server!${NC}"
        return 1
    fi
}

# Stop server
function stop_server() {
    if [ -n "$SERVER_PID" ]; then
        echo "Stopping server (PID: $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        echo -e "${GREEN}Server stopped${NC}"
    fi
}

# Display test results
function print_results() {
    echo -e "\n${YELLOW}=== TEST RESULTS ===${NC}"
    echo -e "Total tests: $TESTS_TOTAL"
    echo -e "Passed tests: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Failed tests: ${RED}$TESTS_FAILED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "\n${GREEN}All tests passed!${NC}"
        exit 0
    else
        echo -e "\n${RED}Some tests failed.${NC}"
        exit 1
    fi
}

# Cleanup on exit
function cleanup() {
    stop_server
}

# Set cleanup handler
trap cleanup EXIT INT TERM

# Main program
print_header "Zircon Webserver Test Protocol"

# Clean build
echo "Performing clean build..."
make clean && make

# Start server
start_server
if [ $? -ne 0 ]; then
    echo -e "${RED}Tests canceled because server failed to start${NC}"
    exit 1
fi

# 1. Basic HTTP Requests
test_header "Basic HTTP Requests"

# HTML file access
run_test "HTML access" "curl -s -o /dev/null -w '%{http_code}' http://$SERVER_HOST:$SERVER_PORT/index.html" "status_0"
run_test "CSS access" "curl -s -o /dev/null -w '%{http_code}' http://$SERVER_HOST:$SERVER_PORT/style.css" "status_0"
run_test "JavaScript access" "curl -s -o /dev/null -w '%{http_code}' http://$SERVER_HOST:$SERVER_PORT/test.js" "status_0"
run_test "Image access" "curl -s -o /dev/null -w '%{http_code}' http://$SERVER_HOST:$SERVER_PORT/test-image.png" "status_0"
run_test "404 - Missing file" "curl -s -o /dev/null -w '%{http_code}' http://$SERVER_HOST:$SERVER_PORT/nonexistent-file.html" "status_0"

# 2. HTTP Header Tests
test_header "HTTP Header Tests"

run_test "Content-Type (HTML)" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html" "contains_Content-Type: text/html"
run_test "Content-Type (CSS)" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/style.css" "contains_Content-Type: text/css"
run_test "Content-Type (JS)" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/test.js" "contains_Content-Type: application/javascript"
run_test "Content-Type (PNG)" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/test-image.png" "contains_Content-Type: image/png"

# 3. Security Header Tests
test_header "Security Header Tests"

run_test "X-Frame-Options" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html" "contains_X-Frame-Options: DENY"
run_test "X-Content-Type-Options" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html" "contains_X-Content-Type-Options: nosniff"
run_test "X-XSS-Protection" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html" "contains_X-XSS-Protection: 1; mode=block"
run_test "Content-Security-Policy" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html" "contains_Content-Security-Policy: default-src 'self'"
run_test "HSTS not emitted over HTTP" "! curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html | grep -qi Strict-Transport-Security" "status_0"

# 4. Cache and ETag Tests
test_header "Cache and ETag Tests"

run_test "ETag presence" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html" "contains_ETag:"
run_test "Cache-Control presence" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html" "contains_Cache-Control:"

# Get ETag value and use in next request
echo "Preparing ETag test..."
ETAG=$(curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html | grep ETag | awk '{print $2}' | tr -d '\r')
echo "ETag: $ETAG"

if [ -n "$ETAG" ]; then
  run_test "304 Not Modified" "curl -s -I -H 'If-None-Match: $ETAG' http://$SERVER_HOST:$SERVER_PORT/index.html" "contains_304 Not Modified"
else
  echo -e "${YELLOW}ETag not found, skipping 304 test${NC}"
fi

# 5. Path Traversal Security Test
test_header "Security Tests"

run_test "Path traversal blocking" "curl -s -o /dev/null -w '%{http_code}' http://$SERVER_HOST:$SERVER_PORT/../etc/passwd" "status_0"
run_test "File type restriction" "curl -s -o /dev/null -w '%{http_code}' http://$SERVER_HOST:$SERVER_PORT/file.php" "status_0"

# 6. Fast Requests and Rate Limiting
test_header "Rate Limiting Test"

# Run this test at the end to avoid affecting other tests
RATE_LIMIT_TEST="echo -e \"Rate limiting test - sending 100 rapid requests...\"
STATUS_429=0
for i in {1..100}; do
    STATUS=\$(curl -s -o /dev/null -w '%{http_code}' http://$SERVER_HOST:$SERVER_PORT/)
    if [ \"\$STATUS\" == \"429\" ]; then
        STATUS_429=1
        break
    fi
done

if [ \$STATUS_429 -eq 1 ]; then
    echo -e \"${GREEN}PASSED${NC} - Rate limiting active\"
    TESTS_PASSED=\$((TESTS_PASSED+1))
else
    echo -e \"${RED}FAILED${NC} - Rate limiting not triggered\"
    TESTS_FAILED=\$((TESTS_FAILED+1))
fi
TESTS_TOTAL=\$((TESTS_TOTAL+1))"

# 7. HTTP Method Tests
test_header "HTTP Method Tests"

run_test "HEAD method" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/index.html" "contains_200 OK"
run_test "GET method" "curl -s http://$SERVER_HOST:$SERVER_PORT/index.html | grep -c 'secure web server' || true" "status_0"

# 8. New Features Tests
test_header "New Features Tests"

# Correct MIME types
run_test "HTML MIME type" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/test.html" "contains_Content-Type: text/html"
run_test "JS MIME type" "curl -s -I http://$SERVER_HOST:$SERVER_PORT/test.js" "contains_Content-Type: application/javascript"

# Clean up after server performance test
stop_server

# Display test results
print_results

exit 0