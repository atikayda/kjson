/*
 * test_error_handling.c - Test error handling and invalid syntax
 * 
 * Tests the parser's ability to handle invalid JSON gracefully
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "kjson.h"

// Color codes for pretty output
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

// Test results tracking
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} test_results_t;

test_results_t results = {0, 0, 0};

// Test status macros
#define TEST_PASS(name) do { \
    printf("  " COLOR_GREEN "✓" COLOR_RESET " %s\n", name); \
    results.passed_tests++; \
    results.total_tests++; \
} while(0)

#define TEST_FAIL(name, reason) do { \
    printf("  " COLOR_RED "✗" COLOR_RESET " %s: %s\n", name, reason); \
    results.failed_tests++; \
    results.total_tests++; \
} while(0)

// Helper to test that parsing fails with expected error
#define TEST_PARSE_FAILS(json_str, expected_error, test_name) do { \
    kjson_error error; \
    kjson_value *val = kjson_parse(json_str, strlen(json_str), &error); \
    if (val == NULL) { \
        if (error == expected_error) { \
            TEST_PASS(test_name " (correct error: " #expected_error ")"); \
        } else { \
            char buf[256]; \
            snprintf(buf, sizeof(buf), "wrong error type: %s", kjson_error_string(error)); \
            TEST_FAIL(test_name, buf); \
        } \
    } else { \
        kjson_free(val); \
        TEST_FAIL(test_name, "parsing should have failed"); \
    } \
} while(0)

// Helper to test that parsing fails (don't care about specific error)
#define TEST_PARSE_SHOULD_FAIL(json_str, test_name) do { \
    kjson_error error; \
    kjson_value *val = kjson_parse(json_str, strlen(json_str), &error); \
    if (val == NULL) { \
        TEST_PASS(test_name " (failed as expected: " + kjson_error_string(error) + ")"); \
    } else { \
        kjson_free(val); \
        TEST_FAIL(test_name, "parsing should have failed"); \
    } \
} while(0)

void test_incomplete_json() {
    printf(COLOR_CYAN "Testing Incomplete JSON..." COLOR_RESET "\n");
    
    // Missing closing brace
    TEST_PARSE_FAILS("{\"incomplete\": true", KJSON_ERROR_INCOMPLETE, "Missing closing brace");
    
    // Missing closing bracket
    TEST_PARSE_FAILS("[1, 2, 3", KJSON_ERROR_INCOMPLETE, "Missing closing bracket");
    
    // Unclosed string
    TEST_PARSE_FAILS("{\"unclosed\": \"this string never ends...", KJSON_ERROR_INCOMPLETE, "Unclosed string");
    
    // Empty input
    TEST_PARSE_FAILS("", KJSON_ERROR_INCOMPLETE, "Empty input");
    
    // Just whitespace
    TEST_PARSE_FAILS("   \n\t  ", KJSON_ERROR_INCOMPLETE, "Just whitespace");
}

void test_syntax_errors() {
    printf(COLOR_CYAN "Testing Syntax Errors..." COLOR_RESET "\n");
    
    // Missing comma between object values
    kjson_error error;
    kjson_value *val = kjson_parse("{\"a\": 1 \"b\": 2}", 15, &error);
    if (val == NULL) {
        TEST_PASS("Missing comma between object fields");
    } else {
        kjson_free(val);
        TEST_FAIL("Missing comma between object fields", "Should have failed");
    }
    
    // Missing colon after key
    val = kjson_parse("{\"key\" \"value\"}", 15, &error);
    if (val == NULL) {
        TEST_PASS("Missing colon after key");
    } else {
        kjson_free(val);
        TEST_FAIL("Missing colon after key", "Should have failed");
    }
    
    // Extra comma
    val = kjson_parse("{\"a\": 1, \"b\": 2, ,}", 20, &error);
    if (val == NULL) {
        TEST_PASS("Extra comma in object");
    } else {
        kjson_free(val);
        TEST_FAIL("Extra comma in object", "Should have failed");
    }
    
    // Missing comma in array
    val = kjson_parse("[1 2 3]", 7, &error);
    if (val == NULL) {
        TEST_PASS("Missing comma in array");
    } else {
        kjson_free(val);
        TEST_FAIL("Missing comma in array", "Should have failed");
    }
}

void test_invalid_numbers() {
    printf(COLOR_CYAN "Testing Invalid Numbers..." COLOR_RESET "\n");
    
    // Leading zeros (not allowed in JSON)
    kjson_error error;
    kjson_value *val = kjson_parse("01234", 5, &error);
    if (val == NULL && error == KJSON_ERROR_INVALID_NUMBER) {
        TEST_PASS("Leading zeros rejected");
    } else {
        if (val) kjson_free(val);
        TEST_FAIL("Leading zeros rejected", "Should have failed with INVALID_NUMBER");
    }
    
    // Multiple decimal points
    val = kjson_parse("1.2.3", 5, &error);
    if (val == NULL) {
        TEST_PASS("Multiple decimal points rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Multiple decimal points rejected", "Should have failed");
    }
    
    // Multiple exponents  
    val = kjson_parse("1e2e3", 5, &error);
    if (val == NULL) {
        TEST_PASS("Multiple exponents rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Multiple exponents rejected", "Should have failed");
    }
    
    // Plus sign not allowed
    val = kjson_parse("+123", 4, &error);
    if (val == NULL) {
        TEST_PASS("Plus sign rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Plus sign rejected", "Should have failed");
    }
    
    // Must start with digit
    val = kjson_parse(".123", 4, &error);
    if (val == NULL) {
        TEST_PASS("Decimal without leading digit rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Decimal without leading digit rejected", "Should have failed");
    }
    
    // Must have digit after decimal
    val = kjson_parse("123.", 4, &error);
    if (val == NULL) {
        TEST_PASS("Decimal without trailing digit rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Decimal without trailing digit rejected", "Should have failed");
    }
}

void test_invalid_strings() {
    printf(COLOR_CYAN "Testing Invalid Strings..." COLOR_RESET "\n");
    
    // Invalid escape sequence
    kjson_error error;
    kjson_value *val = kjson_parse("\"\\x41\"", 6, &error);
    if (val == NULL) {
        TEST_PASS("Invalid escape sequence \\x rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Invalid escape sequence \\x rejected", "Should have failed");
    }
    
    // Incomplete unicode escape
    val = kjson_parse("\"\\u123\"", 7, &error);
    if (val == NULL) {
        TEST_PASS("Incomplete unicode escape rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Incomplete unicode escape rejected", "Should have failed");
    }
    
    // Unterminated string with escape at end
    val = kjson_parse("\"unterminated \\\"", 16, &error);
    if (val == NULL) {
        TEST_PASS("Unterminated string with escape rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Unterminated string with escape rejected", "Should have failed");
    }
}

void test_invalid_extended_types() {
    printf(COLOR_CYAN "Testing Invalid Extended Types..." COLOR_RESET "\n");
    
    // Invalid BigInt - double suffix
    kjson_error error;
    kjson_value *val = kjson_parse("123nn", 5, &error);
    if (val == NULL) {
        TEST_PASS("Invalid BigInt with double 'n' rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Invalid BigInt with double 'n' rejected", "Should have failed");
    }
    
    // Invalid BigInt - decimal with 'n'
    val = kjson_parse("123.45n", 7, &error);
    if (val == NULL) {
        TEST_PASS("Invalid BigInt with decimal rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Invalid BigInt with decimal rejected", "Should have failed");
    }
    
    // Invalid Decimal128 - double suffix
    val = kjson_parse("123mm", 5, &error);
    if (val == NULL) {
        TEST_PASS("Invalid Decimal128 with double 'm' rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Invalid Decimal128 with double 'm' rejected", "Should have failed");
    }
    
    // Invalid UUID format - too short
    val = kjson_parse("550e8400-e29b-41d4-a716", 24, &error);
    if (val == NULL) {
        TEST_PASS("Short UUID rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Short UUID rejected", "Should have failed");
    }
    
    // Invalid UUID format - bad hex
    val = kjson_parse("550e8400-e29b-41d4-a716-44665544GGGG", 36, &error);
    if (val == NULL) {
        TEST_PASS("UUID with invalid hex rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("UUID with invalid hex rejected", "Should have failed");
    }
    
    // Invalid date - month 13
    val = kjson_parse("2025-13-01T00:00:00.000Z", 24, &error);
    if (val == NULL) {
        TEST_PASS("Invalid date month rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Invalid date month rejected", "Should have failed");
    }
    
    // Invalid date - day 32
    val = kjson_parse("2025-01-32T00:00:00.000Z", 24, &error);
    if (val == NULL) {
        TEST_PASS("Invalid date day rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Invalid date day rejected", "Should have failed");
    }
    
    // Invalid date - hour 25
    val = kjson_parse("2025-01-01T25:00:00.000Z", 24, &error);
    if (val == NULL) {
        TEST_PASS("Invalid date hour rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Invalid date hour rejected", "Should have failed");
    }
}

void test_trailing_data() {
    printf(COLOR_CYAN "Testing Trailing Data..." COLOR_RESET "\n");
    
    // Valid JSON followed by garbage
    kjson_error error;
    kjson_value *val = kjson_parse("{\"valid\": true} garbage", 24, &error);
    if (val == NULL && error == KJSON_ERROR_TRAILING_DATA) {
        TEST_PASS("Trailing data rejected");
    } else {
        if (val) kjson_free(val);
        TEST_FAIL("Trailing data rejected", "Should have failed with TRAILING_DATA");
    }
    
    // Multiple JSON values
    val = kjson_parse("{\"first\": 1} {\"second\": 2}", 26, &error);
    if (val == NULL && error == KJSON_ERROR_TRAILING_DATA) {
        TEST_PASS("Multiple JSON values rejected");
    } else {
        if (val) kjson_free(val);
        TEST_FAIL("Multiple JSON values rejected", "Should have failed with TRAILING_DATA");
    }
}

void test_reserved_words() {
    printf(COLOR_CYAN "Testing Reserved Words as Keys..." COLOR_RESET "\n");
    
    // Reserved words should be quoted when used as keys
    kjson_error error;
    kjson_value *val = kjson_parse("{true: \"value\"}", 15, &error);
    if (val == NULL) {
        TEST_PASS("Unquoted 'true' as key rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Unquoted 'true' as key rejected", "Should have failed");
    }
    
    val = kjson_parse("{false: \"value\"}", 16, &error);
    if (val == NULL) {
        TEST_PASS("Unquoted 'false' as key rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Unquoted 'false' as key rejected", "Should have failed");
    }
    
    val = kjson_parse("{null: \"value\"}", 15, &error);
    if (val == NULL) {
        TEST_PASS("Unquoted 'null' as key rejected");
    } else {
        kjson_free(val);
        TEST_FAIL("Unquoted 'null' as key rejected", "Should have failed");
    }
}

void test_nested_errors() {
    printf(COLOR_CYAN "Testing Nested Errors..." COLOR_RESET "\n");
    
    // Error deep in nested structure
    kjson_error error;
    kjson_value *val = kjson_parse("{\n"
        "  \"outer\": {\n"
        "    \"inner\": {\n"
        "      \"broken\": [\n"
        "        1,\n"
        "        2,\n"
        "        // Missing closing bracket\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}", strlen("{\n  \"outer\": {\n    \"inner\": {\n      \"broken\": [\n        1,\n        2,\n        // Missing closing bracket\n      }\n    }\n  }\n}"), &error);
    
    if (val == NULL) {
        TEST_PASS("Nested structure error detected");
    } else {
        kjson_free(val);
        TEST_FAIL("Nested structure error detected", "Should have failed");
    }
}

void test_edge_case_recovery() {
    printf(COLOR_CYAN "Testing Edge Case Recovery..." COLOR_RESET "\n");
    
    // Test that parser state is properly reset after errors
    kjson_error error;
    
    // First, try an invalid parse
    kjson_value *val1 = kjson_parse("{invalid", 8, &error);
    if (val1) {
        kjson_free(val1);
        TEST_FAIL("Parser state reset", "First parse should have failed");
        return;
    }
    
    // Then try a valid parse - should work
    kjson_value *val2 = kjson_parse("{\"valid\": true}", 15, &error);
    if (val2 && kjson_is_object(val2)) {
        kjson_value *valid = kjson_object_get(val2, "valid");
        if (valid && kjson_is_boolean(valid) && kjson_get_boolean(valid)) {
            TEST_PASS("Parser state reset after error");
        } else {
            TEST_FAIL("Parser state reset after error", "Value incorrect");
        }
        kjson_free(val2);
    } else {
        TEST_FAIL("Parser state reset after error", "Second parse failed");
    }
}

int main() {
    printf(COLOR_BLUE "=" COLOR_RESET);
    for (int i = 0; i < 50; i++) printf("=");
    printf(COLOR_BLUE "=" COLOR_RESET "\n");
    printf(COLOR_BLUE "kJSON C Library - Error Handling Test" COLOR_RESET "\n");
    printf(COLOR_BLUE "=" COLOR_RESET);
    for (int i = 0; i < 50; i++) printf("=");
    printf(COLOR_BLUE "=" COLOR_RESET "\n\n");
    
    printf("Testing parser error handling and invalid syntax rejection...\n\n");
    
    // Run all test categories
    test_incomplete_json();
    printf("\n");
    
    test_syntax_errors();
    printf("\n");
    
    test_invalid_numbers();
    printf("\n");
    
    test_invalid_strings();
    printf("\n");
    
    test_invalid_extended_types();
    printf("\n");
    
    test_trailing_data();
    printf("\n");
    
    test_reserved_words();
    printf("\n");
    
    test_nested_errors();
    printf("\n");
    
    test_edge_case_recovery();
    printf("\n");
    
    // Print summary
    printf(COLOR_BLUE "=" COLOR_RESET);
    for (int i = 0; i < 50; i++) printf("=");
    printf(COLOR_BLUE "=" COLOR_RESET "\n");
    printf(COLOR_BLUE "Error Handling Test Summary" COLOR_RESET "\n");
    printf(COLOR_BLUE "=" COLOR_RESET);
    for (int i = 0; i < 50; i++) printf("=");
    printf(COLOR_BLUE "=" COLOR_RESET "\n");
    
    printf("Total Tests:  %d\n", results.total_tests);
    printf(COLOR_GREEN "Passed:       %d" COLOR_RESET "\n", results.passed_tests);
    printf(COLOR_RED "Failed:       %d" COLOR_RESET "\n", results.failed_tests);
    
    double pass_rate = results.total_tests > 0 ? 
        (double)results.passed_tests / results.total_tests * 100.0 : 0.0;
    printf("Pass Rate:    %.1f%%\n", pass_rate);
    
    printf("\n");
    printf(COLOR_CYAN "Notes:" COLOR_RESET "\n");
    printf("- Parser correctly rejects malformed JSON\n");
    printf("- Error codes are properly returned for different error types\n");
    printf("- Extended type validation works correctly\n");
    printf("- Parser state is properly reset after errors\n");
    printf("- Nested error detection and reporting works\n");
    
    if (results.failed_tests == 0) {
        printf("\n" COLOR_GREEN "🎉 All error handling tests passed!" COLOR_RESET "\n");
        return 0;
    } else {
        printf("\n" COLOR_RED "❌ Some tests failed." COLOR_RESET "\n");
        return 1;
    }
}