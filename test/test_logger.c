#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../include/logger.h"

/* Test utilities */
#define TEST(name) static bool test_##name(void)
#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "ASSERTION FAILED: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return false; \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

/* Helper function to check if file exists */
static bool file_exists(const char *filename) {
    struct stat st;
    return stat(filename, &st) == 0;
}

/* Helper function to read file content */
static char *read_file_content(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *content = malloc(length + 1);
    if (content) {
        fread(content, 1, length, file);
        content[length] = '\0';
    }
    
    fclose(file);
    return content;
}

/* Test logger initialization */
TEST(logger_init_file) {
    const char *test_log = "/tmp/test_zircon.log";
    
    // Remove test log if it exists
    unlink(test_log);
    
    log_init(test_log);
    log_write(LOG_INFO, "Test message");
    log_close();
    
    // Check if log file was created
    ASSERT_TRUE(file_exists(test_log));
    
    // Check if message was written
    char *content = read_file_content(test_log);
    ASSERT_TRUE(content != NULL);
    ASSERT_TRUE(strstr(content, "Test message") != NULL);
    
    free(content);
    unlink(test_log);
    
    return true;
}

/* Test logger with NULL filename */
TEST(logger_init_null) {
    // Should not crash with NULL filename
    log_init(NULL);
    log_write(LOG_INFO, "Test message");
    log_close();
    
    return true;
}

/* Test different log levels */
TEST(logger_levels) {
    const char *test_log = "/tmp/test_zircon_levels.log";
    
    unlink(test_log);
    
    log_init(test_log);
    log_set_level(LOG_DEBUG);
    
    log_write(LOG_DEBUG, "Debug message");
    log_write(LOG_INFO, "Info message");
    log_write(LOG_WARNING, "Warning message");
    log_write(LOG_ERROR, "Error message");
    
    log_close();
    
    char *content = read_file_content(test_log);
    ASSERT_TRUE(content != NULL);
    ASSERT_TRUE(strstr(content, "Debug message") != NULL);
    ASSERT_TRUE(strstr(content, "Info message") != NULL);
    ASSERT_TRUE(strstr(content, "Warning message") != NULL);
    ASSERT_TRUE(strstr(content, "Error message") != NULL);
    
    free(content);
    unlink(test_log);
    
    return true;
}

/* Test log level filtering */
TEST(logger_level_filtering) {
    const char *test_log = "/tmp/test_zircon_filter.log";
    
    unlink(test_log);
    
    log_init(test_log);
    log_set_level(LOG_WARNING);  // Only WARNING and ERROR should be logged
    
    log_write(LOG_DEBUG, "Debug message");
    log_write(LOG_INFO, "Info message");
    log_write(LOG_WARNING, "Warning message");
    log_write(LOG_ERROR, "Error message");
    
    log_close();
    
    char *content = read_file_content(test_log);
    ASSERT_TRUE(content != NULL);
    ASSERT_TRUE(strstr(content, "Debug message") == NULL);
    ASSERT_TRUE(strstr(content, "Info message") == NULL);
    ASSERT_TRUE(strstr(content, "Warning message") != NULL);
    ASSERT_TRUE(strstr(content, "Error message") != NULL);
    
    free(content);
    unlink(test_log);
    
    return true;
}

/* Test console output */
TEST(logger_console_output) {
    log_init(NULL);
    log_set_output(LOG_OUTPUT_CONSOLE);
    
    // These should go to console (can't easily test output)
    log_write(LOG_INFO, "Console test message");
    log_write(LOG_ERROR, "Console error message");
    
    log_close();
    
    return true;
}

/* Test both file and console output */
TEST(logger_both_outputs) {
    const char *test_log = "/tmp/test_zircon_both.log";
    
    unlink(test_log);
    
    log_init(test_log);
    log_set_output(LOG_OUTPUT_BOTH);
    
    log_write(LOG_INFO, "Both output test");
    log_close();
    
    // Should be in file
    char *content = read_file_content(test_log);
    ASSERT_TRUE(content != NULL);
    ASSERT_TRUE(strstr(content, "Both output test") != NULL);
    
    free(content);
    unlink(test_log);
    
    return true;
}

/* Test formatted logging */
TEST(logger_formatted_messages) {
    const char *test_log = "/tmp/test_zircon_format.log";
    
    unlink(test_log);
    
    log_init(test_log);
    
    log_write(LOG_INFO, "User %s connected from %s:%d", "admin", "192.168.1.100", 8080);
    log_write(LOG_ERROR, "Failed to process request %d: %s", 404, "Not Found");
    log_write(LOG_DEBUG, "Processing %d bytes of data", 1024);
    
    log_close();
    
    char *content = read_file_content(test_log);
    ASSERT_TRUE(content != NULL);
    ASSERT_TRUE(strstr(content, "User admin connected from 192.168.1.100:8080") != NULL);
    ASSERT_TRUE(strstr(content, "Failed to process request 404: Not Found") != NULL);
    ASSERT_TRUE(strstr(content, "Processing 1024 bytes of data") != NULL);
    
    free(content);
    unlink(test_log);
    
    return true;
}

/* Test logging with NULL message */
TEST(logger_null_message) {
    const char *test_log = "/tmp/test_zircon_null.log";
    
    unlink(test_log);
    
    log_init(test_log);
    
    // Should not crash with NULL message
    log_write(LOG_INFO, NULL);
    
    log_close();
    unlink(test_log);
    
    return true;
}

/* Test logging with empty message */
TEST(logger_empty_message) {
    const char *test_log = "/tmp/test_zircon_empty.log";
    
    unlink(test_log);
    
    log_init(test_log);
    
    log_write(LOG_INFO, "");
    
    log_close();
    
    // File should exist but might be empty or have timestamp only
    ASSERT_TRUE(file_exists(test_log));
    
    unlink(test_log);
    
    return true;
}

/* Test very long log messages */
TEST(logger_long_messages) {
    const char *test_log = "/tmp/test_zircon_long.log";
    
    unlink(test_log);
    
    log_init(test_log);
    
    // Create a very long message
    char long_message[5000];
    memset(long_message, 'A', sizeof(long_message) - 1);
    long_message[sizeof(long_message) - 1] = '\0';
    
    log_write(LOG_INFO, "Long message: %s", long_message);
    
    log_close();
    
    char *content = read_file_content(test_log);
    ASSERT_TRUE(content != NULL);
    ASSERT_TRUE(strstr(content, "Long message:") != NULL);
    
    free(content);
    unlink(test_log);
    
    return true;
}

/* Test multiple log files */
TEST(logger_multiple_files) {
    const char *test_log1 = "/tmp/test_zircon_1.log";
    const char *test_log2 = "/tmp/test_zircon_2.log";
    
    unlink(test_log1);
    unlink(test_log2);
    
    // First log file
    log_init(test_log1);
    log_write(LOG_INFO, "Message in file 1");
    log_close();
    
    // Second log file
    log_init(test_log2);
    log_write(LOG_INFO, "Message in file 2");
    log_close();
    
    // Check both files
    char *content1 = read_file_content(test_log1);
    char *content2 = read_file_content(test_log2);
    
    ASSERT_TRUE(content1 != NULL);
    ASSERT_TRUE(content2 != NULL);
    ASSERT_TRUE(strstr(content1, "Message in file 1") != NULL);
    ASSERT_TRUE(strstr(content2, "Message in file 2") != NULL);
    
    free(content1);
    free(content2);
    unlink(test_log1);
    unlink(test_log2);
    
    return true;
}

/* Test log rotation behavior */
TEST(logger_file_permissions) {
    const char *test_log = "/tmp/test_zircon_perms.log";
    
    unlink(test_log);
    
    log_init(test_log);
    log_write(LOG_INFO, "Permission test");
    log_close();
    
    // Check if file was created with reasonable permissions
    struct stat st;
    ASSERT_TRUE(stat(test_log, &st) == 0);
    
    // File should be readable and writable by owner
    ASSERT_TRUE(st.st_mode & S_IRUSR);
    ASSERT_TRUE(st.st_mode & S_IWUSR);
    
    unlink(test_log);
    
    return true;
}

/* Test concurrent logging simulation */
TEST(logger_concurrent_simulation) {
    const char *test_log = "/tmp/test_zircon_concurrent.log";
    
    unlink(test_log);
    
    log_init(test_log);
    
    // Simulate multiple rapid log writes
    for (int i = 0; i < 100; i++) {
        log_write(LOG_INFO, "Concurrent message %d", i);
    }
    
    log_close();
    
    char *content = read_file_content(test_log);
    ASSERT_TRUE(content != NULL);
    
    // Count occurrences of "Concurrent message"
    int count = 0;
    char *ptr = content;
    while ((ptr = strstr(ptr, "Concurrent message")) != NULL) {
        count++;
        ptr++;
    }
    
    ASSERT_TRUE(count == 100);
    
    free(content);
    unlink(test_log);
    
    return true;
}

/* Test runner */
int main(void) {
    printf("Running Logger tests...\n\n");
    
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
    
    RUN_TEST(logger_init_file);
    RUN_TEST(logger_init_null);
    RUN_TEST(logger_levels);
    RUN_TEST(logger_level_filtering);
    RUN_TEST(logger_console_output);
    RUN_TEST(logger_both_outputs);
    RUN_TEST(logger_formatted_messages);
    RUN_TEST(logger_null_message);
    RUN_TEST(logger_empty_message);
    RUN_TEST(logger_long_messages);
    RUN_TEST(logger_multiple_files);
    RUN_TEST(logger_file_permissions);
    RUN_TEST(logger_concurrent_simulation);
    
    printf("\nLogger Tests Summary:\n");
    printf("Failed: %d\n", failed);
    printf("Status: %s\n", failed == 0 ? "PASS" : "FAIL");
    
    return failed;
}

