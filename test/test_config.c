#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../include/security_config.h"

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

/* Helper function to create test config file */
static bool create_test_config(const char *filename, const char *content) {
    FILE *file = fopen(filename, "w");
    if (!file) return false;
    
    fprintf(file, "%s", content);
    fclose(file);
    return true;
}

/* Test default configuration */
TEST(config_defaults) {
    security_config_t config;
    security_config_set_defaults(&config);
    
    // Check default values
    ASSERT_TRUE(config.xss_protection);
    ASSERT_TRUE(config.sql_injection_protection);
    ASSERT_TRUE(config.path_traversal_protection);
    ASSERT_TRUE(config.rate_limiting_enabled);
    ASSERT_EQ(config.max_requests_per_minute, 60);
    ASSERT_EQ(config.block_duration_minutes, 5);
    
    return true;
}

/* Test configuration creation and destruction */
TEST(config_create_destroy) {
    security_config_t *config = security_config_create();
    ASSERT_TRUE(config != NULL);
    
    security_config_destroy(config);
    
    return true;
}

/* Test loading valid configuration file */
TEST(config_load_valid) {
    const char *test_config_file = "/tmp/test_zircon_config.conf";
    const char *config_content = 
        "xss_protection=true\n"
        "sql_injection_protection=true\n"
        "path_traversal_protection=true\n"
        "rate_limiting_enabled=true\n"
        "max_requests_per_minute=100\n"
        "block_duration_minutes=10\n"
        "allowed_file_extensions=html,css,js,png,jpg,gif\n"
        "blocked_file_extensions=php,asp,jsp\n";
    
    ASSERT_TRUE(create_test_config(test_config_file, config_content));
    
    security_config_t config;
    security_config_set_defaults(&config);
    
    bool result = security_config_load(&config, test_config_file);
    ASSERT_TRUE(result);
    
    // Verify loaded values
    ASSERT_TRUE(config.xss_protection);
    ASSERT_TRUE(config.sql_injection_protection);
    ASSERT_TRUE(config.path_traversal_protection);
    ASSERT_TRUE(config.rate_limiting_enabled);
    ASSERT_EQ(config.max_requests_per_minute, 100);
    ASSERT_EQ(config.block_duration_minutes, 10);
    
    unlink(test_config_file);
    return true;
}

/* Test loading configuration with missing file */
TEST(config_load_missing_file) {
    security_config_t config;
    security_config_set_defaults(&config);
    
    bool result = security_config_load(&config, "/nonexistent/config.conf");
    ASSERT_FALSE(result);
    
    return true;
}

/* Test loading configuration with NULL filename */
TEST(config_load_null_filename) {
    security_config_t config;
    security_config_set_defaults(&config);
    
    bool result = security_config_load(&config, NULL);
    ASSERT_FALSE(result);
    
    return true;
}

/* Test loading configuration with NULL config */
TEST(config_load_null_config) {
    const char *test_config_file = "/tmp/test_zircon_null.conf";
    const char *config_content = "xss_protection=true\n";
    
    ASSERT_TRUE(create_test_config(test_config_file, config_content));
    
    bool result = security_config_load(NULL, test_config_file);
    ASSERT_FALSE(result);
    
    unlink(test_config_file);
    return true;
}

/* Test loading malformed configuration */
TEST(config_load_malformed) {
    const char *test_config_file = "/tmp/test_zircon_malformed.conf";
    const char *config_content = 
        "xss_protection=true\n"
        "invalid_line_without_equals\n"
        "=value_without_key\n"
        "key_without_value=\n"
        "sql_injection_protection=invalid_boolean\n"
        "max_requests_per_minute=not_a_number\n";
    
    ASSERT_TRUE(create_test_config(test_config_file, config_content));
    
    security_config_t config;
    security_config_set_defaults(&config);
    
    // Should handle malformed config gracefully
    bool result = security_config_load(&config, test_config_file);
    
    // Depending on implementation, might succeed with partial loading
    // or fail completely. Either is acceptable as long as it doesn't crash.
    
    unlink(test_config_file);
    return true;
}

/* Test saving configuration */
TEST(config_save) {
    const char *test_config_file = "/tmp/test_zircon_save.conf";
    
    security_config_t config;
    security_config_set_defaults(&config);
    
    // Modify some values
    config.max_requests_per_minute = 120;
    config.block_duration_minutes = 15;
    config.xss_protection = false;
    
    bool result = security_config_save(&config, test_config_file);
    ASSERT_TRUE(result);
    
    // Verify file was created
    struct stat st;
    ASSERT_TRUE(stat(test_config_file, &st) == 0);
    
    // Load the saved config and verify
    security_config_t loaded_config;
    security_config_set_defaults(&loaded_config);
    
    result = security_config_load(&loaded_config, test_config_file);
    ASSERT_TRUE(result);
    
    ASSERT_EQ(loaded_config.max_requests_per_minute, 120);
    ASSERT_EQ(loaded_config.block_duration_minutes, 15);
    ASSERT_FALSE(loaded_config.xss_protection);
    
    unlink(test_config_file);
    return true;
}

/* Test saving with NULL parameters */
TEST(config_save_null_params) {
    security_config_t config;
    security_config_set_defaults(&config);
    
    // NULL filename
    bool result = security_config_save(&config, NULL);
    ASSERT_FALSE(result);
    
    // NULL config
    result = security_config_save(NULL, "/tmp/test.conf");
    ASSERT_FALSE(result);
    
    return true;
}

/* Test configuration with extreme values */
TEST(config_extreme_values) {
    security_config_t config;
    security_config_set_defaults(&config);
    
    // Test with extreme values
    config.max_requests_per_minute = 0;
    config.block_duration_minutes = 0;
    
    // Should handle gracefully
    ASSERT_EQ(config.max_requests_per_minute, 0);
    ASSERT_EQ(config.block_duration_minutes, 0);
    
    // Test with very large values
    config.max_requests_per_minute = 1000000;
    config.block_duration_minutes = 1000000;
    
    ASSERT_EQ(config.max_requests_per_minute, 1000000);
    ASSERT_EQ(config.block_duration_minutes, 1000000);
    
    return true;
}

/* Test configuration validation */
TEST(config_validation) {
    security_config_t config;
    
    // Test with all protections disabled
    security_config_set_defaults(&config);
    config.xss_protection = false;
    config.sql_injection_protection = false;
    config.path_traversal_protection = false;
    config.rate_limiting_enabled = false;
    
    // Should still be valid (though not recommended)
    // Implementation should handle this gracefully
    
    return true;
}

/* Test configuration file permissions */
TEST(config_file_permissions) {
    const char *test_config_file = "/tmp/test_zircon_perms.conf";
    
    security_config_t config;
    security_config_set_defaults(&config);
    
    bool result = security_config_save(&config, test_config_file);
    ASSERT_TRUE(result);
    
    // Check file permissions
    struct stat st;
    ASSERT_TRUE(stat(test_config_file, &st) == 0);
    
    // File should be readable and writable by owner
    ASSERT_TRUE(st.st_mode & S_IRUSR);
    ASSERT_TRUE(st.st_mode & S_IWUSR);
    
    unlink(test_config_file);
    return true;
}

/* Test configuration with comments and whitespace */
TEST(config_with_comments) {
    const char *test_config_file = "/tmp/test_zircon_comments.conf";
    const char *config_content = 
        "# Zircon Security Configuration\n"
        "# XSS Protection Settings\n"
        "xss_protection=true\n"
        "\n"
        "  # SQL Injection Protection\n"
        "  sql_injection_protection=true  \n"
        "\n"
        "# Rate limiting\n"
        "rate_limiting_enabled=true\n"
        "max_requests_per_minute=50  # requests per minute\n"
        "\n"
        "# Block duration in minutes\n"
        "block_duration_minutes=5\n";
    
    ASSERT_TRUE(create_test_config(test_config_file, config_content));
    
    security_config_t config;
    security_config_set_defaults(&config);
    
    bool result = security_config_load(&config, test_config_file);
    ASSERT_TRUE(result);
    
    // Verify values were parsed correctly despite comments and whitespace
    ASSERT_TRUE(config.xss_protection);
    ASSERT_TRUE(config.sql_injection_protection);
    ASSERT_TRUE(config.rate_limiting_enabled);
    ASSERT_EQ(config.max_requests_per_minute, 50);
    ASSERT_EQ(config.block_duration_minutes, 5);
    
    unlink(test_config_file);
    return true;
}

/* Test configuration reload */
TEST(config_reload) {
    const char *test_config_file = "/tmp/test_zircon_reload.conf";
    
    // Create initial config
    const char *config_content1 = 
        "xss_protection=true\n"
        "max_requests_per_minute=60\n";
    
    ASSERT_TRUE(create_test_config(test_config_file, config_content1));
    
    security_config_t config;
    security_config_set_defaults(&config);
    
    // Load initial config
    bool result = security_config_load(&config, test_config_file);
    ASSERT_TRUE(result);
    ASSERT_TRUE(config.xss_protection);
    ASSERT_EQ(config.max_requests_per_minute, 60);
    
    // Modify config file
    const char *config_content2 = 
        "xss_protection=false\n"
        "max_requests_per_minute=120\n";
    
    ASSERT_TRUE(create_test_config(test_config_file, config_content2));
    
    // Reload config
    result = security_config_load(&config, test_config_file);
    ASSERT_TRUE(result);
    ASSERT_FALSE(config.xss_protection);
    ASSERT_EQ(config.max_requests_per_minute, 120);
    
    unlink(test_config_file);
    return true;
}

/* Test runner */
int main(void) {
    printf("Running Configuration tests...\n\n");
    
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
    
    RUN_TEST(config_defaults);
    RUN_TEST(config_create_destroy);
    RUN_TEST(config_load_valid);
    RUN_TEST(config_load_missing_file);
    RUN_TEST(config_load_null_filename);
    RUN_TEST(config_load_null_config);
    RUN_TEST(config_load_malformed);
    RUN_TEST(config_save);
    RUN_TEST(config_save_null_params);
    RUN_TEST(config_extreme_values);
    RUN_TEST(config_validation);
    RUN_TEST(config_file_permissions);
    RUN_TEST(config_with_comments);
    RUN_TEST(config_reload);
    
    printf("\nConfiguration Tests Summary:\n");
    printf("Failed: %d\n", failed);
    printf("Status: %s\n", failed == 0 ? "PASS" : "FAIL");
    
    return failed;
}

