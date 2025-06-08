CC = gcc
CFLAGS = -Wall -Wextra -pedantic -I./include
DEBUG ?= 0

ifeq ($(DEBUG), 1)
    CFLAGS += -g -O0 -DDEBUG
else
    CFLAGS += -O2 -DNDEBUG
endif

LDFLAGS = -lpthread

SRC_DIR = src
TEST_DIR = test
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TEST_SRCS = $(wildcard $(TEST_DIR)/*.c)
TEST_OBJS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(OBJ_DIR)/%.o)

TARGET = $(BIN_DIR)/zircon
TEST_TARGET = $(BIN_DIR)/test_suite

all: setup $(TARGET)

# Original test target for backward compatibility
test-original: setup $(TEST_TARGET)
	@echo "Running original test suite..."
	@./$(TEST_TARGET)

setup:
	@mkdir -p $(OBJ_DIR) $(BIN_DIR) www

$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	@$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build complete: $@"

$(TEST_TARGET): $(TEST_OBJS) $(filter-out $(OBJ_DIR)/main.o, $(OBJS))
	@echo "Linking $(TEST_TARGET)..."
	@$(CC) $^ -o $@ $(LDFLAGS)
	@echo "Test build complete: $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(TEST_DIR)/%.c
	@echo "Compiling $<..."
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "Cleaning build files..."
	@rm -rf $(OBJ_DIR)/* $(BIN_DIR)/*

install: $(TARGET)
	@echo "Installing to /usr/local/bin (requires sudo)..."
	@sudo cp $(TARGET) /usr/local/bin/
	@sudo mkdir -p /usr/local/share/zircon
	@sudo cp -r www/* /usr/local/share/zircon/
	@echo "Installation complete"

uninstall:
	@echo "Uninstalling (requires sudo)..."
	@sudo rm -f /usr/local/bin/zircon
	@sudo rm -rf /usr/local/share/zircon
	@echo "Uninstallation complete"

distclean: clean
	@echo "Removing all generated files and directories..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)

help:
	@echo "Available targets:"
	@echo "  all            - Build the server (default)"
	@echo "  test           - Run comprehensive test suite"
	@echo "  test-original  - Build and run original tests"
	@echo "  test-unit      - Run unit tests only"
	@echo "  test-security  - Run security tests only"
	@echo "  test-performance - Run performance tests only"
	@echo "  test-integration - Run integration tests only"
	@echo "  test-memory    - Run memory tests with valgrind"
	@echo "  test-stress    - Run stress tests"
	@echo "  test-build     - Build test binaries only"
	@echo "  test-clean     - Clean test artifacts"
	@echo "  test-help      - Show detailed test help"
	@echo "  clean          - Remove object files and binaries"
	@echo "  distclean      - Remove all generated files and directories"
	@echo "  install        - Install to /usr/local/bin (requires sudo)"
	@echo "  uninstall      - Remove from /usr/local/bin (requires sudo)"
	@echo "  help           - Show this help message"
	@echo ""
	@echo "Options:"
	@echo "  DEBUG=1        - Build with debug symbols and without optimization"

# Test targets
.PHONY: test test-unit test-security test-performance test-integration test-memory test-stress test-clean test-build test-help

# Run all tests (comprehensive test suite)
test:
	@echo "Running comprehensive test suite..."
	@./test/run_all_tests.sh

# Run unit tests only
test-unit:
	@echo "Running unit tests..."
	@./test/run_all_tests.sh unit

# Run security tests only
test-security:
	@echo "Running security tests..."
	@./test/run_all_tests.sh security

# Run performance tests only
test-performance:
	@echo "Running performance tests..."
	@./test/run_all_tests.sh performance

# Run integration tests only
test-integration:
	@echo "Running integration tests..."
	@./test/run_all_tests.sh integration

# Run memory tests with valgrind
test-memory:
	@echo "Running memory tests..."
	@./test/run_all_tests.sh memory

# Run stress tests
test-stress:
	@echo "Running stress tests..."
	@./test/stress_test.sh

# Build test binaries only
test-build:
	@echo "Building test binaries..."
	@./test/run_all_tests.sh build

# Clean test artifacts
test-clean:
	@echo "Cleaning test artifacts..."
	@./test/run_all_tests.sh clean
	@rm -rf /tmp/zircon_test_logs/
	@rm -f obj/test_*

# Help target for tests
test-help:
	@echo "Available test targets:"
	@echo "  test           - Run all tests (unit, security, performance, integration)"
	@echo "  test-unit      - Run unit tests only"
	@echo "  test-security  - Run security tests only"
	@echo "  test-performance - Run performance tests only"
	@echo "  test-integration - Run integration tests only"
	@echo "  test-memory    - Run memory tests with valgrind"
	@echo "  test-stress    - Run stress tests"
	@echo "  test-build     - Build test binaries only"
	@echo "  test-clean     - Clean test artifacts"
	@echo "  test-help      - Show this help message"
	@echo ""
	@echo "Test categories:"
	@echo "  Unit Tests     - Fast, isolated tests for individual functions"
	@echo "  Security Tests - Comprehensive vulnerability and attack vector testing"
	@echo "  Performance    - Load testing and performance benchmarks"
	@echo "  Integration    - End-to-end functionality testing"
	@echo "  Memory Tests   - Memory leak detection with valgrind"
	@echo "  Stress Tests   - High-load and resource exhaustion testing"

.PHONY: all clean setup test-original install uninstall distclean help

