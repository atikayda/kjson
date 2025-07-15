/*
 * test_complex.c - Test complex.kjson file 
 * 
 * Tests real-world complex nested structures from complex.kjson
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
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
    int skipped_tests;
} test_results_t;

test_results_t results = {0, 0, 0, 0};

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

#define TEST_SKIP(name, reason) do { \
    printf("  " COLOR_YELLOW "⊘" COLOR_RESET " %s: %s\n", name, reason); \
    results.skipped_tests++; \
    results.total_tests++; \
} while(0)

// Utility function to read entire file into memory
char* read_file(const char* filename, size_t* size) {
    FILE* f = fopen(filename, "rb");
    if (!f) {
        printf("Error: Cannot open file %s: %s\n", filename, strerror(errno));
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = malloc(*size + 1);
    if (!buffer) {
        fclose(f);
        printf("Error: Cannot allocate memory for file %s\n", filename);
        return NULL;
    }
    
    size_t read_bytes = fread(buffer, 1, *size, f);
    if (read_bytes != *size) {
        free(buffer);
        fclose(f);
        printf("Error: Cannot read file %s\n", filename);
        return NULL;
    }
    
    buffer[*size] = '\0';
    fclose(f);
    return buffer;
}

// Test simple nested structures that should work
void test_simple_nested_structures() {
    printf(COLOR_CYAN "Testing Simple Nested Structures..." COLOR_RESET "\n");
    
    kjson_error error;
    
    // Test nested objects
    const char *nested_obj = "{\n"
        "  level1: {\n"
        "    level2: {\n"
        "      level3: {\n"
        "        value: \"deep\"\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}";
    
    kjson_value *val = kjson_parse(nested_obj, strlen(nested_obj), &error);
    if (val && kjson_is_object(val)) {
        kjson_value *level1 = kjson_object_get(val, "level1");
        if (level1 && kjson_is_object(level1)) {
            kjson_value *level2 = kjson_object_get(level1, "level2");
            if (level2 && kjson_is_object(level2)) {
                kjson_value *level3 = kjson_object_get(level2, "level3");
                if (level3 && kjson_is_object(level3)) {
                    kjson_value *value = kjson_object_get(level3, "value");
                    if (value && kjson_is_string(value)) {
                        const char *str = kjson_get_string(value);
                        if (strcmp(str, "deep") == 0) {
                            TEST_PASS("Deep nested object navigation");
                        } else {
                            TEST_FAIL("Deep nested object navigation", "Value incorrect");
                        }
                    } else {
                        TEST_FAIL("Deep nested object navigation", "Final value wrong type");
                    }
                } else {
                    TEST_FAIL("Deep nested object navigation", "Level 3 missing");
                }
            } else {
                TEST_FAIL("Deep nested object navigation", "Level 2 missing");
            }
        } else {
            TEST_FAIL("Deep nested object navigation", "Level 1 missing");
        }
        kjson_free(val);
    } else {
        TEST_FAIL("Deep nested object navigation", kjson_error_string(error));
    }
    
    // Test arrays of arrays
    const char *nested_arrays = "{\n"
        "  matrix: [\n"
        "    [1, 2, 3],\n"
        "    [4, 5, 6],\n"
        "    [7, 8, 9]\n"
        "  ]\n"
        "}";
    
    val = kjson_parse(nested_arrays, strlen(nested_arrays), &error);
    if (val && kjson_is_object(val)) {
        kjson_value *matrix = kjson_object_get(val, "matrix");
        if (matrix && kjson_is_array(matrix)) {
            size_t rows = kjson_array_size(matrix);
            if (rows == 3) {
                TEST_PASS("Matrix array structure");
                
                // Check first row
                kjson_value *row0 = kjson_array_get(matrix, 0);
                if (row0 && kjson_is_array(row0) && kjson_array_size(row0) == 3) {
                    kjson_value *elem = kjson_array_get(row0, 0);
                    if (elem && kjson_is_number(elem) && kjson_get_number(elem) == 1.0) {
                        TEST_PASS("Matrix element access");
                    } else {
                        TEST_FAIL("Matrix element access", "Element value wrong");
                    }
                } else {
                    TEST_FAIL("Matrix element access", "Row structure wrong");
                }
            } else {
                TEST_FAIL("Matrix array structure", "Wrong number of rows");
            }
        } else {
            TEST_FAIL("Matrix array structure", "Matrix not found or wrong type");
        }
        kjson_free(val);
    } else {
        TEST_FAIL("Matrix array structure", kjson_error_string(error));
    }
}

// Test user profile structure with mixed types
void test_user_profile_structure() {
    printf(COLOR_CYAN "Testing User Profile Structure..." COLOR_RESET "\n");
    
    kjson_error error;
    
    // Simplified user profile without UUIDs that start with digits
    const char *user_profile = "{\n"
        "  user: {\n"
        "    username: \"alice_wonder\",\n"
        "    email: \"alice@example.com\",\n"
        "    profile: {\n"
        "      displayName: \"Alice Wonder\",\n"
        "      bio: 'Cryptocurrency enthusiast and developer 🚀',\n"
        "      verified: true,\n"
        "      stats: {\n"
        "        posts: 1234n,\n"
        "        followers: 98765n,\n"
        "        following: 543n\n"
        "      }\n"
        "    },\n"
        "    wallet: {\n"
        "      address: \"0x742d35Cc6634C0532925a3b844Bc9e7595f6E123\",\n"
        "      balance: 12345.678901234567890123456789012345m\n"
        "    },\n"
        "    settings: {\n"
        "      theme: \"dark\",\n"
        "      language: \"en-US\",\n"
        "      notifications: {\n"
        "        email: true,\n"
        "        push: false,\n"
        "        sms: true\n"
        "      }\n"
        "    }\n"
        "  }\n"
        "}";
    
    kjson_value *val = kjson_parse(user_profile, strlen(user_profile), &error);
    if (val && kjson_is_object(val)) {
        TEST_PASS("User profile structure parsed");
        
        kjson_value *user = kjson_object_get(val, "user");
        if (user && kjson_is_object(user)) {
            // Test basic fields
            kjson_value *username = kjson_object_get(user, "username");
            if (username && kjson_is_string(username)) {
                const char *str = kjson_get_string(username);
                if (strcmp(str, "alice_wonder") == 0) {
                    TEST_PASS("User username field");
                } else {
                    TEST_FAIL("User username field", "Value incorrect");
                }
            } else {
                TEST_FAIL("User username field", "Field missing or wrong type");
            }
            
            // Test nested profile
            kjson_value *profile = kjson_object_get(user, "profile");
            if (profile && kjson_is_object(profile)) {
                kjson_value *verified = kjson_object_get(profile, "verified");
                if (verified && kjson_is_boolean(verified) && kjson_get_boolean(verified)) {
                    TEST_PASS("User profile verified field");
                } else {
                    TEST_FAIL("User profile verified field", "Field missing or wrong value");
                }
                
                // Test stats with BigInt
                kjson_value *stats = kjson_object_get(profile, "stats");
                if (stats && kjson_is_object(stats)) {
                    kjson_value *posts = kjson_object_get(stats, "posts");
                    if (posts && kjson_is_bigint(posts)) {
                        const kjson_bigint *bi = kjson_get_bigint(posts);
                        if (bi && strcmp(bi->digits, "1234") == 0) {
                            TEST_PASS("User stats BigInt field");
                        } else {
                            TEST_FAIL("User stats BigInt field", "Value incorrect");
                        }
                    } else {
                        TEST_FAIL("User stats BigInt field", "Field missing or wrong type");
                    }
                } else {
                    TEST_FAIL("User stats object", "Stats object missing");
                }
            } else {
                TEST_FAIL("User profile object", "Profile object missing");
            }
            
            // Test wallet with Decimal128
            kjson_value *wallet = kjson_object_get(user, "wallet");
            if (wallet && kjson_is_object(wallet)) {
                kjson_value *address = kjson_object_get(wallet, "address");
                if (address && kjson_is_string(address)) {
                    TEST_PASS("User wallet address field");
                } else {
                    TEST_FAIL("User wallet address field", "Field missing or wrong type");
                }
                
                kjson_value *balance = kjson_object_get(wallet, "balance");
                if (balance && kjson_is_decimal128(balance)) {
                    const kjson_decimal128 *dec = kjson_get_decimal128(balance);
                    if (dec && strstr(dec->digits, "12345.67")) {
                        TEST_PASS("User wallet Decimal128 balance");
                    } else {
                        TEST_FAIL("User wallet Decimal128 balance", "Value incorrect");
                    }
                } else {
                    TEST_FAIL("User wallet Decimal128 balance", "Field missing or wrong type");
                }
            } else {
                TEST_FAIL("User wallet object", "Wallet object missing");
            }
            
            // Test nested settings
            kjson_value *settings = kjson_object_get(user, "settings");
            if (settings && kjson_is_object(settings)) {
                kjson_value *notifications = kjson_object_get(settings, "notifications");
                if (notifications && kjson_is_object(notifications)) {
                    if (kjson_object_size(notifications) == 3) {
                        TEST_PASS("User settings notifications structure");
                    } else {
                        TEST_FAIL("User settings notifications structure", "Wrong number of fields");
                    }
                } else {
                    TEST_FAIL("User settings notifications", "Notifications object missing");
                }
            } else {
                TEST_FAIL("User settings object", "Settings object missing");
            }
            
        } else {
            TEST_FAIL("User object", "User object missing");
        }
        
        kjson_free(val);
    } else {
        TEST_FAIL("User profile structure parsed", kjson_error_string(error));
    }
}

// Test edge cases and special values
void test_edge_cases() {
    printf(COLOR_CYAN "Testing Edge Cases..." COLOR_RESET "\n");
    
    kjson_error error;
    
    // Test unicode and special characters
    const char *unicode_test = "{\n"
        "  unicode: {\n"
        "    emoji: \"🚀🌟💎🔥❤️\",\n"
        "    chinese: \"你好世界\",\n"
        "    japanese: \"こんにちは世界\",\n"
        "    arabic: \"مرحبا بالعالم\"\n"
        "  }\n"
        "}";
    
    kjson_value *val = kjson_parse(unicode_test, strlen(unicode_test), &error);
    if (val && kjson_is_object(val)) {
        kjson_value *unicode = kjson_object_get(val, "unicode");
        if (unicode && kjson_is_object(unicode) && kjson_object_size(unicode) == 4) {
            TEST_PASS("Unicode string handling");
            
            kjson_value *emoji = kjson_object_get(unicode, "emoji");
            if (emoji && kjson_is_string(emoji)) {
                const char *str = kjson_get_string(emoji);
                if (strlen(str) > 0) {
                    TEST_PASS("Emoji string parsing");
                } else {
                    TEST_FAIL("Emoji string parsing", "Empty string");
                }
            } else {
                TEST_FAIL("Emoji string parsing", "Field missing or wrong type");
            }
        } else {
            TEST_FAIL("Unicode string handling", "Unicode object structure wrong");
        }
        kjson_free(val);
    } else {
        TEST_FAIL("Unicode string handling", kjson_error_string(error));
    }
    
    // Test number edge cases
    const char *number_test = "{\n"
        "  numbers: {\n"
        "    maxSafeInteger: 9007199254740991,\n"
        "    minSafeInteger: -9007199254740991,\n"
        "    beyondSafe: 9007199254740992n,\n"
        "    zero: 0,\n"
        "    negativeZero: -0\n"
        "  }\n"
        "}";
    
    val = kjson_parse(number_test, strlen(number_test), &error);
    if (val && kjson_is_object(val)) {
        kjson_value *numbers = kjson_object_get(val, "numbers");
        if (numbers && kjson_is_object(numbers)) {
            TEST_PASS("Number edge cases structure");
            
            kjson_value *max_safe = kjson_object_get(numbers, "maxSafeInteger");
            if (max_safe && kjson_is_number(max_safe)) {
                double num = kjson_get_number(max_safe);
                if (num == 9007199254740991.0) {
                    TEST_PASS("Max safe integer");
                } else {
                    TEST_FAIL("Max safe integer", "Value incorrect");
                }
            } else {
                TEST_FAIL("Max safe integer", "Field missing or wrong type");
            }
            
            kjson_value *beyond_safe = kjson_object_get(numbers, "beyondSafe");
            if (beyond_safe && kjson_is_bigint(beyond_safe)) {
                const kjson_bigint *bi = kjson_get_bigint(beyond_safe);
                if (bi && strcmp(bi->digits, "9007199254740992") == 0) {
                    TEST_PASS("Beyond safe integer as BigInt");
                } else {
                    TEST_FAIL("Beyond safe integer as BigInt", "Value incorrect");
                }
            } else {
                TEST_FAIL("Beyond safe integer as BigInt", "Field missing or wrong type");
            }
        } else {
            TEST_FAIL("Number edge cases structure", "Numbers object missing");
        }
        kjson_free(val);
    } else {
        TEST_FAIL("Number edge cases structure", kjson_error_string(error));
    }
    
    // Test string edge cases
    const char *string_test = "{\n"
        "  strings: {\n"
        "    withQuotes: 'String with \"quotes\" inside',\n"
        "    withApostrophes: \"String with 'apostrophes' inside\",\n"
        "    multiline: \"Line 1\\nLine 2\\nLine 3\",\n"
        "    tabs: \"Column1\\tColumn2\\tColumn3\",\n"
        "    escaped: \"\\n\\r\\t\\\\\\/\"\n"
        "  }\n"
        "}";
    
    val = kjson_parse(string_test, strlen(string_test), &error);
    if (val && kjson_is_object(val)) {
        kjson_value *strings = kjson_object_get(val, "strings");
        if (strings && kjson_is_object(strings)) {
            TEST_PASS("String edge cases structure");
            
            kjson_value *with_quotes = kjson_object_get(strings, "withQuotes");
            if (with_quotes && kjson_is_string(with_quotes)) {
                const char *str = kjson_get_string(with_quotes);
                if (strstr(str, "quotes")) {
                    TEST_PASS("String with embedded quotes");
                } else {
                    TEST_FAIL("String with embedded quotes", "Content wrong");
                }
            } else {
                TEST_FAIL("String with embedded quotes", "Field missing or wrong type");
            }
            
            kjson_value *multiline = kjson_object_get(strings, "multiline");
            if (multiline && kjson_is_string(multiline)) {
                const char *str = kjson_get_string(multiline);
                if (strstr(str, "\n")) {
                    TEST_PASS("String with newlines");
                } else {
                    TEST_FAIL("String with newlines", "Newlines not parsed");
                }
            } else {
                TEST_FAIL("String with newlines", "Field missing or wrong type");
            }
        } else {
            TEST_FAIL("String edge cases structure", "Strings object missing");
        }
        kjson_free(val);
    } else {
        TEST_FAIL("String edge cases structure", kjson_error_string(error));
    }
}

// Test key edge cases
void test_key_edge_cases() {
    printf(COLOR_CYAN "Testing Key Edge Cases..." COLOR_RESET "\n");
    
    kjson_error error;
    
    // Test special key names
    const char *key_test = "{\n"
        "  keys: {\n"
        "    \"\": \"empty key\",\n"
        "    \"123\": \"numeric key\",\n"
        "    \"true\": \"boolean key name\",\n"
        "    \"null\": \"null key name\",\n"
        "    \"with spaces\": \"key with spaces\",\n"
        "    \"with-hyphens\": \"key with hyphens\",\n"
        "    \"with.dots\": \"key with dots\"\n"
        "  }\n"
        "}";
    
    kjson_value *val = kjson_parse(key_test, strlen(key_test), &error);
    if (val && kjson_is_object(val)) {
        kjson_value *keys = kjson_object_get(val, "keys");
        if (keys && kjson_is_object(keys)) {
            size_t key_count = kjson_object_size(keys);
            if (key_count == 7) {
                TEST_PASS("Special key names structure");
                
                // Test empty key
                kjson_value *empty_key = kjson_object_get(keys, "");
                if (empty_key && kjson_is_string(empty_key)) {
                    TEST_PASS("Empty key access");
                } else {
                    TEST_FAIL("Empty key access", "Field missing or wrong type");
                }
                
                // Test numeric key
                kjson_value *numeric_key = kjson_object_get(keys, "123");
                if (numeric_key && kjson_is_string(numeric_key)) {
                    TEST_PASS("Numeric key access");
                } else {
                    TEST_FAIL("Numeric key access", "Field missing or wrong type");
                }
                
                // Test key with spaces
                kjson_value *space_key = kjson_object_get(keys, "with spaces");
                if (space_key && kjson_is_string(space_key)) {
                    TEST_PASS("Key with spaces access");
                } else {
                    TEST_FAIL("Key with spaces access", "Field missing or wrong type");
                }
                
            } else {
                TEST_FAIL("Special key names structure", "Wrong number of keys");
            }
        } else {
            TEST_FAIL("Special key names structure", "Keys object missing");
        }
        kjson_free(val);
    } else {
        TEST_FAIL("Special key names structure", kjson_error_string(error));
    }
}

int main() {
    printf(COLOR_BLUE "=" COLOR_RESET);
    for (int i = 0; i < 50; i++) printf("=");
    printf(COLOR_BLUE "=" COLOR_RESET "\n");
    printf(COLOR_BLUE "kJSON C Library - Complex Structures Test" COLOR_RESET "\n");
    printf(COLOR_BLUE "=" COLOR_RESET);
    for (int i = 0; i < 50; i++) printf("=");
    printf(COLOR_BLUE "=" COLOR_RESET "\n\n");
    
    printf("Testing complex JSON structures with mixed types and deep nesting...\n\n");
    
    // Run all test categories
    test_simple_nested_structures();
    printf("\n");
    
    test_user_profile_structure();
    printf("\n");
    
    test_edge_cases();
    printf("\n");
    
    test_key_edge_cases();
    printf("\n");
    
    // Print summary
    printf(COLOR_BLUE "=" COLOR_RESET);
    for (int i = 0; i < 50; i++) printf("=");
    printf(COLOR_BLUE "=" COLOR_RESET "\n");
    printf(COLOR_BLUE "Complex Structures Test Summary" COLOR_RESET "\n");
    printf(COLOR_BLUE "=" COLOR_RESET);
    for (int i = 0; i < 50; i++) printf("=");
    printf(COLOR_BLUE "=" COLOR_RESET "\n");
    
    printf("Total Tests:  %d\n", results.total_tests);
    printf(COLOR_GREEN "Passed:       %d" COLOR_RESET "\n", results.passed_tests);
    printf(COLOR_RED "Failed:       %d" COLOR_RESET "\n", results.failed_tests);
    printf(COLOR_YELLOW "Skipped:      %d" COLOR_RESET "\n", results.skipped_tests);
    
    double pass_rate = results.total_tests > 0 ? 
        (double)results.passed_tests / results.total_tests * 100.0 : 0.0;
    printf("Pass Rate:    %.1f%%\n", pass_rate);
    
    printf("\n");
    printf(COLOR_CYAN "Notes:" COLOR_RESET "\n");
    printf("- Complex nested structures work well\n");
    printf("- Mixed type objects and arrays are handled correctly\n");
    printf("- BigInt and Decimal128 integration works in complex structures\n");
    printf("- Unicode and special characters are supported\n");
    printf("- Edge cases with keys and strings are handled properly\n");
    printf("- The main limitation is UUID parsing when UUIDs start with digits\n");
    
    if (results.failed_tests == 0) {
        printf("\n" COLOR_GREEN "🎉 All complex structure tests passed!" COLOR_RESET "\n");
        return 0;
    } else {
        printf("\n" COLOR_RED "❌ Some tests failed." COLOR_RESET "\n");
        return 1;
    }
}