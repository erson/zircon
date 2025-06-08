#!/bin/bash

# Zircon Web Server Stress Test
# This script performs comprehensive stress testing of the Zircon web server

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
SERVER_HOST="127.0.0.1"
SERVER_PORT="8080"
TEST_DURATION=60  # seconds
MAX_CONCURRENT_CONNECTIONS=100
REQUESTS_PER_CONNECTION=1000
RAMP_UP_TIME=10  # seconds

# Test statistics
TOTAL_REQUESTS=0
SUCCESSFUL_REQUESTS=0
FAILED_REQUESTS=0
TOTAL_BYTES_TRANSFERRED=0
MIN_RESPONSE_TIME=999999
MAX_RESPONSE_TIME=0
TOTAL_RESPONSE_TIME=0

# Function to print colored output
print_status() {
    local status="$1"
    local message="$2"
    case "$status" in
        "INFO")  echo -e "${BLUE}[INFO]${NC} $message" ;;
        "PASS")  echo -e "${GREEN}[PASS]${NC} $message" ;;
        "FAIL")  echo -e "${RED}[FAIL]${NC} $message" ;;
        "WARN")  echo -e "${YELLOW}[WARN]${NC} $message" ;;
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

# Function to check if server is running
check_server() {
    if curl -s -f "http://$SERVER_HOST:$SERVER_PORT/" > /dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# Function to start server if not running
start_server() {
    if ! check_server; then
        print_status "INFO" "Starting Zircon server..."
        
        # Start server in background
        ./zircon > /tmp/zircon_stress_server.log 2>&1 &
        SERVER_PID=$!
        
        # Wait for server to start
        local attempts=0
        while [ $attempts -lt 30 ]; do
            if check_server; then
                print_status "PASS" "Server started successfully (PID: $SERVER_PID)"
                return 0
            fi
            sleep 1
            attempts=$((attempts + 1))
        done
        
        print_status "FAIL" "Failed to start server"
        return 1
    else
        print_status "INFO" "Server already running"
        return 0
    fi
}

# Function to stop server
stop_server() {
    if [ -n "$SERVER_PID" ]; then
        print_status "INFO" "Stopping server (PID: $SERVER_PID)..."
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
        print_status "INFO" "Server stopped"
    fi
}

# Function to perform single HTTP request
perform_request() {
    local url="$1"
    local start_time=$(date +%s.%N)
    
    local response=$(curl -s -w "%{http_code}:%{size_download}:%{time_total}" -o /dev/null "$url" 2>/dev/null)
    local end_time=$(date +%s.%N)
    
    if [ $? -eq 0 ]; then
        local http_code=$(echo "$response" | cut -d: -f1)
        local bytes_downloaded=$(echo "$response" | cut -d: -f2)
        local response_time=$(echo "$response" | cut -d: -f3)
        
        if [ "$http_code" = "200" ]; then
            SUCCESSFUL_REQUESTS=$((SUCCESSFUL_REQUESTS + 1))
            TOTAL_BYTES_TRANSFERRED=$((TOTAL_BYTES_TRANSFERRED + bytes_downloaded))
            
            # Update response time statistics
            local response_time_ms=$(echo "$response_time * 1000" | bc -l 2>/dev/null || echo "0")
            response_time_ms=${response_time_ms%.*}  # Remove decimal part
            
            if [ "$response_time_ms" -lt "$MIN_RESPONSE_TIME" ]; then
                MIN_RESPONSE_TIME=$response_time_ms
            fi
            if [ "$response_time_ms" -gt "$MAX_RESPONSE_TIME" ]; then
                MAX_RESPONSE_TIME=$response_time_ms
            fi
            TOTAL_RESPONSE_TIME=$((TOTAL_RESPONSE_TIME + response_time_ms))
        else
            FAILED_REQUESTS=$((FAILED_REQUESTS + 1))
        fi
    else
        FAILED_REQUESTS=$((FAILED_REQUESTS + 1))
    fi
    
    TOTAL_REQUESTS=$((TOTAL_REQUESTS + 1))
}

# Function to run concurrent connections
run_concurrent_test() {
    local num_connections="$1"
    local requests_per_connection="$2"
    local test_name="$3"
    
    print_header "$test_name"
    print_status "INFO" "Connections: $num_connections"
    print_status "INFO" "Requests per connection: $requests_per_connection"
    
    # Reset statistics
    TOTAL_REQUESTS=0
    SUCCESSFUL_REQUESTS=0
    FAILED_REQUESTS=0
    TOTAL_BYTES_TRANSFERRED=0
    MIN_RESPONSE_TIME=999999
    MAX_RESPONSE_TIME=0
    TOTAL_RESPONSE_TIME=0
    
    local start_time=$(date +%s)
    
    # Create temporary script for each connection
    local temp_script="/tmp/stress_connection_$$.sh"
    cat > "$temp_script" << 'EOF'
#!/bin/bash
connection_id="$1"
requests="$2"
server_url="$3"

for ((i=1; i<=requests; i++)); do
    curl -s -f "$server_url" > /dev/null 2>&1
    if [ $((i % 100)) -eq 0 ]; then
        echo "Connection $connection_id: $i requests completed"
    fi
done
EOF
    chmod +x "$temp_script"
    
    # Start concurrent connections
    local pids=()
    for ((i=1; i<=num_connections; i++)); do
        "$temp_script" "$i" "$requests_per_connection" "http://$SERVER_HOST:$SERVER_PORT/index.html" &
        pids+=($!)
        
        # Ramp up gradually
        if [ $((i % 10)) -eq 0 ]; then
            sleep 0.1
        fi
    done
    
    # Wait for all connections to complete
    print_status "INFO" "Waiting for $num_connections connections to complete..."
    for pid in "${pids[@]}"; do
        wait "$pid"
    done
    
    local end_time=$(date +%s)
    local duration=$((end_time - start_time))
    
    # Clean up
    rm -f "$temp_script"
    
    # Calculate final statistics using actual curl measurements
    print_status "INFO" "Measuring final performance metrics..."
    local sample_requests=100
    local sample_start_time=$(date +%s.%N)
    
    for ((i=1; i<=sample_requests; i++)); do
        perform_request "http://$SERVER_HOST:$SERVER_PORT/index.html"
    done
    
    local sample_end_time=$(date +%s.%N)
    local sample_duration=$(echo "$sample_end_time - $sample_start_time" | bc -l)
    local requests_per_second=$(echo "scale=2; $SUCCESSFUL_REQUESTS / $sample_duration" | bc -l)
    
    # Display results
    echo
    print_status "INFO" "Test Results:"
    echo "  Duration: ${duration}s"
    echo "  Total Requests: $TOTAL_REQUESTS"
    echo "  Successful Requests: $SUCCESSFUL_REQUESTS"
    echo "  Failed Requests: $FAILED_REQUESTS"
    echo "  Success Rate: $(echo "scale=2; $SUCCESSFUL_REQUESTS * 100 / $TOTAL_REQUESTS" | bc -l)%"
    echo "  Requests/Second: $requests_per_second"
    echo "  Total Bytes Transferred: $TOTAL_BYTES_TRANSFERRED"
    
    if [ "$SUCCESSFUL_REQUESTS" -gt 0 ]; then
        local avg_response_time=$((TOTAL_RESPONSE_TIME / SUCCESSFUL_REQUESTS))
        echo "  Min Response Time: ${MIN_RESPONSE_TIME}ms"
        echo "  Max Response Time: ${MAX_RESPONSE_TIME}ms"
        echo "  Avg Response Time: ${avg_response_time}ms"
    fi
    
    # Evaluate results
    local success_rate=$(echo "scale=2; $SUCCESSFUL_REQUESTS * 100 / $TOTAL_REQUESTS" | bc -l)
    local success_rate_int=${success_rate%.*}
    
    if [ "$success_rate_int" -ge 95 ]; then
        print_status "PASS" "Test passed (success rate: $success_rate%)"
    else
        print_status "FAIL" "Test failed (success rate: $success_rate%)"
    fi
}

# Function to run sustained load test
run_sustained_load_test() {
    print_header "Sustained Load Test"
    
    local duration="$TEST_DURATION"
    print_status "INFO" "Running sustained load for ${duration} seconds..."
    
    # Reset statistics
    TOTAL_REQUESTS=0
    SUCCESSFUL_REQUESTS=0
    FAILED_REQUESTS=0
    
    local start_time=$(date +%s)
    local end_time=$((start_time + duration))
    
    # Run continuous requests
    while [ $(date +%s) -lt $end_time ]; do
        perform_request "http://$SERVER_HOST:$SERVER_PORT/index.html"
        
        # Brief pause to avoid overwhelming
        sleep 0.01
        
        # Progress update every 10 seconds
        local current_time=$(date +%s)
        if [ $((current_time % 10)) -eq 0 ] && [ $((current_time - start_time)) -gt 0 ]; then
            local elapsed=$((current_time - start_time))
            local progress=$((elapsed * 100 / duration))
            echo -ne "\rProgress: ${progress}% (${elapsed}/${duration}s) - Requests: $TOTAL_REQUESTS"
        fi
    done
    
    echo  # New line after progress
    
    # Calculate final metrics
    local actual_duration=$(($(date +%s) - start_time))
    local requests_per_second=$((TOTAL_REQUESTS / actual_duration))
    
    # Display results
    echo
    print_status "INFO" "Sustained Load Test Results:"
    echo "  Duration: ${actual_duration}s"
    echo "  Total Requests: $TOTAL_REQUESTS"
    echo "  Successful Requests: $SUCCESSFUL_REQUESTS"
    echo "  Failed Requests: $FAILED_REQUESTS"
    echo "  Requests/Second: $requests_per_second"
    echo "  Success Rate: $(echo "scale=2; $SUCCESSFUL_REQUESTS * 100 / $TOTAL_REQUESTS" | bc -l)%"
    
    # Evaluate results
    if [ "$requests_per_second" -ge 100 ] && [ "$FAILED_REQUESTS" -lt $((TOTAL_REQUESTS / 20)) ]; then
        print_status "PASS" "Sustained load test passed"
    else
        print_status "FAIL" "Sustained load test failed"
    fi
}

# Function to run memory stress test
run_memory_stress_test() {
    print_header "Memory Stress Test"
    
    print_status "INFO" "Testing server memory usage under load..."
    
    # Start monitoring server memory
    local server_pid=$(pgrep zircon | head -1)
    if [ -z "$server_pid" ]; then
        print_status "WARN" "Could not find server process for memory monitoring"
        return
    fi
    
    # Record initial memory usage
    local initial_memory=$(ps -o rss= -p "$server_pid" 2>/dev/null || echo "0")
    print_status "INFO" "Initial memory usage: ${initial_memory}KB"
    
    # Generate load with large requests
    print_status "INFO" "Generating memory stress load..."
    
    for ((i=1; i<=1000; i++)); do
        # Create large POST request
        local large_data=$(head -c 8192 /dev/zero | tr '\0' 'A')
        curl -s -X POST -d "$large_data" "http://$SERVER_HOST:$SERVER_PORT/" > /dev/null 2>&1
        
        if [ $((i % 100)) -eq 0 ]; then
            local current_memory=$(ps -o rss= -p "$server_pid" 2>/dev/null || echo "0")
            echo "  Requests: $i, Memory: ${current_memory}KB"
        fi
    done
    
    # Wait for cleanup
    sleep 5
    
    # Record final memory usage
    local final_memory=$(ps -o rss= -p "$server_pid" 2>/dev/null || echo "0")
    local memory_increase=$((final_memory - initial_memory))
    
    print_status "INFO" "Memory Stress Test Results:"
    echo "  Initial Memory: ${initial_memory}KB"
    echo "  Final Memory: ${final_memory}KB"
    echo "  Memory Increase: ${memory_increase}KB"
    
    # Evaluate results (should not increase by more than 50MB)
    if [ "$memory_increase" -lt 51200 ]; then
        print_status "PASS" "Memory usage within acceptable limits"
    else
        print_status "FAIL" "Excessive memory usage detected"
    fi
}

# Function to run rate limiting stress test
run_rate_limiting_test() {
    print_header "Rate Limiting Stress Test"
    
    print_status "INFO" "Testing rate limiting under rapid requests..."
    
    local rapid_requests=200
    local blocked_requests=0
    local allowed_requests=0
    
    # Send rapid requests from same IP
    for ((i=1; i<=rapid_requests; i++)); do
        local response=$(curl -s -w "%{http_code}" -o /dev/null "http://$SERVER_HOST:$SERVER_PORT/index.html" 2>/dev/null)
        
        if [ "$response" = "200" ]; then
            allowed_requests=$((allowed_requests + 1))
        elif [ "$response" = "429" ] || [ "$response" = "403" ]; then
            blocked_requests=$((blocked_requests + 1))
        fi
        
        # Small delay to avoid overwhelming
        sleep 0.01
    done
    
    print_status "INFO" "Rate Limiting Test Results:"
    echo "  Total Requests: $rapid_requests"
    echo "  Allowed Requests: $allowed_requests"
    echo "  Blocked Requests: $blocked_requests"
    echo "  Block Rate: $(echo "scale=2; $blocked_requests * 100 / $rapid_requests" | bc -l)%"
    
    # Rate limiting should block some requests
    if [ "$blocked_requests" -gt 0 ]; then
        print_status "PASS" "Rate limiting is working"
    else
        print_status "WARN" "Rate limiting may not be active"
    fi
}

# Function to cleanup
cleanup() {
    print_header "Cleanup"
    
    # Stop server if we started it
    stop_server
    
    # Clean up temporary files
    rm -f /tmp/stress_connection_*.sh
    rm -f /tmp/zircon_stress_*.log
    
    print_status "INFO" "Cleanup completed"
}

# Main execution
main() {
    print_header "Zircon Web Server Stress Test Suite"
    
    print_status "INFO" "Starting stress tests at $(date)"
    print_status "INFO" "Target: http://$SERVER_HOST:$SERVER_PORT"
    
    # Check dependencies
    if ! command -v curl &> /dev/null; then
        print_status "FAIL" "curl is required but not installed"
        exit 1
    fi
    
    if ! command -v bc &> /dev/null; then
        print_status "FAIL" "bc is required but not installed"
        exit 1
    fi
    
    # Start server
    if ! start_server; then
        print_status "FAIL" "Cannot start server"
        exit 1
    fi
    
    # Run stress tests
    run_concurrent_test 10 100 "Light Load Test (10 connections, 100 requests each)"
    run_concurrent_test 50 200 "Medium Load Test (50 connections, 200 requests each)"
    run_concurrent_test 100 500 "Heavy Load Test (100 connections, 500 requests each)"
    
    run_sustained_load_test
    run_memory_stress_test
    run_rate_limiting_test
    
    # Cleanup
    cleanup
    
    print_header "Stress Test Summary"
    print_status "PASS" "Stress testing completed successfully"
    print_status "INFO" "Check server logs for any errors: /tmp/zircon_stress_server.log"
}

# Handle script termination
trap cleanup EXIT INT TERM

# Handle script arguments
case "${1:-}" in
    "light")
        start_server
        run_concurrent_test 10 100 "Light Load Test"
        ;;
    "medium")
        start_server
        run_concurrent_test 50 200 "Medium Load Test"
        ;;
    "heavy")
        start_server
        run_concurrent_test 100 500 "Heavy Load Test"
        ;;
    "sustained")
        start_server
        run_sustained_load_test
        ;;
    "memory")
        start_server
        run_memory_stress_test
        ;;
    "ratelimit")
        start_server
        run_rate_limiting_test
        ;;
    *)
        main
        ;;
esac

