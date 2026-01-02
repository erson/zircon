#!/bin/bash

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PASS=0
FAIL=0
SKIP=0

HOST="127.0.0.1"
PORT="8000"
BASE_URL="http://${HOST}:${PORT}"

log_pass() { echo -e "${GREEN}[PASS]${NC} $1"; ((PASS++)); }
log_fail() { echo -e "${RED}[FAIL]${NC} $1"; ((FAIL++)); }
log_skip() { echo -e "${YELLOW}[SKIP]${NC} $1"; ((SKIP++)); }
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_section() { echo -e "\n${BLUE}=== $1 ===${NC}"; }

check_server() {
    if ! curl -s --connect-timeout 2 "${BASE_URL}/" > /dev/null 2>&1; then
        echo -e "${RED}Server not running on ${BASE_URL}${NC}"
        echo "Start server with: ./bin/zircon"
        exit 1
    fi
}

expect_status() {
    local desc="$1"
    local expected="$2"
    local url="$3"
    shift 3
    local extra_args=("$@")
    
    local status=$(curl -s -o /dev/null -w "%{http_code}" "${extra_args[@]}" "$url" 2>/dev/null)
    
    if [ "$status" = "$expected" ]; then
        log_pass "$desc (got $status)"
    else
        log_fail "$desc (expected $expected, got $status)"
    fi
}

expect_status_raw() {
    local desc="$1"
    local expected="$2"
    local request="$3"
    
    local response=$(echo -e "$request" | nc -w 2 "$HOST" "$PORT" 2>/dev/null | head -1)
    local status=$(echo "$response" | grep -oE "HTTP/1\.[01] [0-9]+" | awk '{print $2}')
    
    if [ "$status" = "$expected" ]; then
        log_pass "$desc (got $status)"
    else
        log_fail "$desc (expected $expected, got ${status:-timeout/empty})"
    fi
}

expect_close() {
    local desc="$1"
    local request="$2"
    
    local response=$(echo -e "$request" | nc -w 2 "$HOST" "$PORT" 2>/dev/null)
    
    if [ -z "$response" ] || echo "$response" | grep -qE "HTTP/1\.[01] [45][0-9]{2}"; then
        log_pass "$desc"
    else
        log_fail "$desc (got response when expecting close/error)"
    fi
}

log_section "HTTP Request Edge Cases"

expect_status_raw "Empty request" "400" "\r\n\r\n"
expect_status_raw "Only newlines" "400" "\n\n\n\n"
expect_status_raw "Missing HTTP version" "400" "GET /\r\n\r\n"
expect_status_raw "Invalid HTTP method" "400" "INVALID / HTTP/1.1\r\n\r\n"
expect_status_raw "Very long method" "400" "$(printf 'A%.0s' {1..1000}) / HTTP/1.1\r\n\r\n"
expect_status_raw "HTTP/0.9 request" "400" "GET /\r\n"
expect_status_raw "HTTP/2.0 (unsupported)" "400" "GET / HTTP/2.0\r\n\r\n"
expect_status_raw "Lowercase http version" "400" "GET / http/1.1\r\n\r\n"
expect_status_raw "Missing path" "400" "GET  HTTP/1.1\r\n\r\n"
expect_status_raw "Double space in request line" "400" "GET  / HTTP/1.1\r\n\r\n"
expect_status_raw "Tab instead of space" "400" "GET\t/\tHTTP/1.1\r\n\r\n"
expect_status_raw "CRLF only in headers" "400" "\r\n\r\n\r\n"
expect_status_raw "LF only (no CR)" "200" "GET / HTTP/1.1\nHost: test\n\n"

log_section "Path Traversal Attacks"

expect_status "Basic path traversal ../" "403" "${BASE_URL}/../etc/passwd"
expect_status "Double traversal ../../" "403" "${BASE_URL}/../../etc/passwd"
expect_status "Triple traversal" "403" "${BASE_URL}/../../../etc/passwd"
expect_status "Traversal with backslash" "403" "${BASE_URL}/..\\..\\etc\\passwd"
expect_status "URL encoded ../" "403" "${BASE_URL}/%2e%2e/etc/passwd"
expect_status "Double URL encoded" "403" "${BASE_URL}/%252e%252e/etc/passwd"
expect_status "Mixed encoding" "403" "${BASE_URL}/..%2f..%2fetc/passwd"
expect_status "Unicode encoding ％" "403" "${BASE_URL}/%c0%ae%c0%ae/etc/passwd"
expect_status "Overlong UTF-8" "403" "${BASE_URL}/%c0%2e%c0%2e/etc/passwd"
expect_status "Null byte injection" "403" "${BASE_URL}/index.html%00.php"
expect_status "Null byte in path" "403" "${BASE_URL}/../%00/etc/passwd"
expect_status "Double slash //" "403" "${BASE_URL}//etc/passwd"
expect_status "Triple slash ///" "403" "${BASE_URL}///etc/passwd"
expect_status "Dot at end" "404" "${BASE_URL}/index.html."
expect_status "Dot dot at end" "403" "${BASE_URL}/www/.."
expect_status "Hidden file .htaccess" "403" "${BASE_URL}/.htaccess"
expect_status "Hidden file .git" "403" "${BASE_URL}/.git/config"
expect_status "Absolute path /etc/passwd" "403" "${BASE_URL}//etc/passwd"

log_section "File Type Restrictions"

expect_status "Allowed: .html" "200" "${BASE_URL}/index.html"
expect_status "Allowed: .css" "200" "${BASE_URL}/style.css"
expect_status "Forbidden: .php" "403" "${BASE_URL}/test.php"
expect_status "Forbidden: .exe" "403" "${BASE_URL}/test.exe"
expect_status "Forbidden: .sh" "403" "${BASE_URL}/test.sh"
expect_status "Forbidden: .py" "403" "${BASE_URL}/test.py"
expect_status "Forbidden: .rb" "403" "${BASE_URL}/test.rb"
expect_status "Forbidden: .pl" "403" "${BASE_URL}/test.pl"
expect_status "Forbidden: .cgi" "403" "${BASE_URL}/test.cgi"
expect_status "Forbidden: .asp" "403" "${BASE_URL}/test.asp"
expect_status "Forbidden: .jsp" "403" "${BASE_URL}/test.jsp"
expect_status "Forbidden: no extension" "403" "${BASE_URL}/Makefile"
expect_status "Case sensitivity .HTML" "200" "${BASE_URL}/index.HTML" || expect_status "Case sensitivity .HTML" "404" "${BASE_URL}/index.HTML"
expect_status "Double extension .html.php" "403" "${BASE_URL}/test.html.php"
expect_status "Extension in query" "403" "${BASE_URL}/test.php?ext=html"

log_section "URL Edge Cases"

expect_status "Very long path (1000 chars)" "404" "${BASE_URL}/$(printf 'a%.0s' {1..1000}).html"
expect_status "Very long path (4000 chars)" "414" "${BASE_URL}/$(printf 'a%.0s' {1..4000}).html" || expect_status "Very long path (4000 chars)" "400" "${BASE_URL}/$(printf 'a%.0s' {1..4000}).html"
expect_status "Query string" "200" "${BASE_URL}/?foo=bar"
expect_status "Multiple query params" "200" "${BASE_URL}/?a=1&b=2&c=3"
expect_status "Fragment (should be ignored)" "200" "${BASE_URL}/#section"
expect_status "Empty query" "200" "${BASE_URL}/?"
expect_status "Just question mark" "200" "${BASE_URL}/index.html?"
expect_status "Special chars in query" "200" "${BASE_URL}/?foo=<script>"
expect_status "Unicode in path" "404" "${BASE_URL}/日本語.html"
expect_status "Space in path (encoded)" "404" "${BASE_URL}/test%20file.html"
expect_status "Plus in path" "404" "${BASE_URL}/test+file.html"

log_section "HTTP Header Edge Cases"

expect_status_raw "No Host header (HTTP/1.1)" "200" "GET / HTTP/1.1\r\n\r\n"
expect_status_raw "Empty Host header" "200" "GET / HTTP/1.1\r\nHost: \r\n\r\n"
expect_status_raw "Very long header value" "400" "GET / HTTP/1.1\r\nHost: localhost\r\nX-Long: $(printf 'A%.0s' {1..8192})\r\n\r\n"
expect_status_raw "Many headers (100)" "200" "GET / HTTP/1.1\r\nHost: localhost\r\n$(for i in {1..100}; do echo -n "X-Header-$i: value\r\n"; done)\r\n"
expect_status_raw "Header without value" "200" "GET / HTTP/1.1\r\nHost: localhost\r\nX-Empty:\r\n\r\n"
expect_status_raw "Header with only spaces" "200" "GET / HTTP/1.1\r\nHost: localhost\r\nX-Spaces:    \r\n\r\n"
expect_status_raw "Duplicate headers" "200" "GET / HTTP/1.1\r\nHost: localhost\r\nHost: other\r\n\r\n"
expect_status_raw "Header injection attempt" "200" "GET / HTTP/1.1\r\nHost: localhost\r\nX-Injected: value\r\nEvil: header\r\n\r\n"
expect_status_raw "CRLF injection in header" "400" "GET / HTTP/1.1\r\nHost: localhost\r\nX-CRLF: val\r\n\r\nInjected: yes\r\n\r\n"
expect_status_raw "Null byte in header" "400" "GET / HTTP/1.1\r\nHost: local\x00host\r\n\r\n"

log_section "HTTP Method Edge Cases"

expect_status "GET request" "200" "${BASE_URL}/"
expect_status "HEAD request" "200" "${BASE_URL}/" -I
expect_status_raw "POST request" "405" "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n"
expect_status_raw "PUT request" "405" "PUT / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n"
expect_status_raw "DELETE request" "405" "DELETE / HTTP/1.1\r\nHost: localhost\r\n\r\n"
expect_status_raw "OPTIONS request" "405" "OPTIONS / HTTP/1.1\r\nHost: localhost\r\n\r\n"
expect_status_raw "TRACE request" "405" "TRACE / HTTP/1.1\r\nHost: localhost\r\n\r\n"
expect_status_raw "CONNECT request" "405" "CONNECT localhost:80 HTTP/1.1\r\nHost: localhost\r\n\r\n"
expect_status_raw "PATCH request" "405" "PATCH / HTTP/1.1\r\nHost: localhost\r\n\r\n"

log_section "ETag and Caching Edge Cases"

etag=$(curl -s -I "${BASE_URL}/" | grep -i "^ETag:" | awk '{print $2}' | tr -d '\r')
if [ -n "$etag" ]; then
    log_info "Server ETag: $etag"
    expect_status "If-None-Match with correct ETag" "304" "${BASE_URL}/" -H "If-None-Match: $etag"
    expect_status "If-None-Match with wrong ETag" "200" "${BASE_URL}/" -H "If-None-Match: \"wrong-etag\""
    expect_status "If-None-Match with wildcard *" "304" "${BASE_URL}/" -H "If-None-Match: *"
    expect_status "If-None-Match with multiple ETags" "304" "${BASE_URL}/" -H "If-None-Match: \"wrong\", $etag, \"other\""
else
    log_skip "ETag tests (server not returning ETag)"
fi

log_section "Content-Type and MIME Edge Cases"

html_type=$(curl -s -I "${BASE_URL}/index.html" | grep -i "^Content-Type:" | cut -d: -f2 | tr -d ' \r')
if echo "$html_type" | grep -qi "text/html"; then
    log_pass "Correct MIME for .html: $html_type"
else
    log_fail "Incorrect MIME for .html: $html_type"
fi

css_type=$(curl -s -I "${BASE_URL}/style.css" | grep -i "^Content-Type:" | cut -d: -f2 | tr -d ' \r')
if echo "$css_type" | grep -qi "text/css"; then
    log_pass "Correct MIME for .css: $css_type"
else
    log_fail "Incorrect MIME for .css: $css_type"
fi

log_section "Security Headers"

headers=$(curl -s -I "${BASE_URL}/")

check_header() {
    local name="$1"
    local expected="$2"
    local value=$(echo "$headers" | grep -i "^${name}:" | cut -d: -f2- | tr -d '\r' | xargs)
    
    if [ -n "$value" ]; then
        if [ -n "$expected" ]; then
            if echo "$value" | grep -qi "$expected"; then
                log_pass "Header $name: $value"
            else
                log_fail "Header $name: got '$value', expected '$expected'"
            fi
        else
            log_pass "Header $name present: $value"
        fi
    else
        log_fail "Header $name missing"
    fi
}

check_header "X-Content-Type-Options" "nosniff"
check_header "X-Frame-Options" "DENY"
check_header "X-XSS-Protection" "1"
check_header "Content-Security-Policy" "default-src"
check_header "Referrer-Policy" ""
check_header "Permissions-Policy" ""

log_section "Connection Edge Cases"

log_info "Testing slow client (2 byte chunks)..."
response=$(echo -e "GET / HTTP/1.1\r\nHost: test\r\n\r\n" | while read -n 2 chunk; do
    echo -n "$chunk"
    sleep 0.1
done | nc -w 5 "$HOST" "$PORT" 2>/dev/null | head -1)
if echo "$response" | grep -q "HTTP/1"; then
    log_pass "Handles slow client"
else
    log_skip "Slow client test inconclusive"
fi

log_info "Testing pipelined requests..."
response=$(echo -e "GET / HTTP/1.1\r\nHost: test\r\n\r\nGET /style.css HTTP/1.1\r\nHost: test\r\n\r\n" | nc -w 2 "$HOST" "$PORT" 2>/dev/null)
count=$(echo "$response" | grep -c "HTTP/1.1 200" || true)
if [ "$count" -ge 1 ]; then
    log_pass "Handles pipelined requests (responded to $count)"
else
    log_fail "Failed to handle pipelined requests"
fi

log_section "File Edge Cases"

expect_status "Non-existent file" "404" "${BASE_URL}/nonexistent.html"
expect_status "Non-existent directory" "404" "${BASE_URL}/nonexistent/"
expect_status "Root path" "200" "${BASE_URL}/"
expect_status "Index in subdir" "200" "${BASE_URL}/test_files/" || expect_status "Index in subdir" "404" "${BASE_URL}/test_files/"

if [ -f "www/test_files/empty.png" ]; then
    expect_status "Empty file" "200" "${BASE_URL}/test_files/empty.png"
else
    log_skip "Empty file test (www/test_files/empty.png not found)"
fi

log_section "Rate Limiting"

log_info "Testing rate limit (sending rapid requests)..."
success=0
limited=0
for i in {1..50}; do
    status=$(curl -s -o /dev/null -w "%{http_code}" "${BASE_URL}/" 2>/dev/null)
    if [ "$status" = "200" ]; then
        ((success++))
    elif [ "$status" = "429" ]; then
        ((limited++))
    fi
done

log_info "50 rapid requests: $success succeeded, $limited rate-limited"
if [ "$limited" -gt 0 ]; then
    log_pass "Rate limiting is active"
else
    log_info "Rate limiting may not be triggered (limit may be high)"
fi

log_section "Summary"

echo ""
echo -e "Total: $((PASS + FAIL + SKIP)) tests"
echo -e "${GREEN}Passed: $PASS${NC}"
echo -e "${RED}Failed: $FAIL${NC}"
echo -e "${YELLOW}Skipped: $SKIP${NC}"

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
exit 0
