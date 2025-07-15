/*
 * test_kjson.c - Test program for kJSON C library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "kjson.h"

void test_basic_types(void)
{
    printf("Testing basic types...\n");
    
    /* Test parsing null */
    kjson_error error;
    kjson_value *val = kjson_parse("null", 4, &error);
    assert(val != NULL);
    assert(kjson_is_null(val));
    kjson_free(val);
    
    /* Test parsing boolean */
    val = kjson_parse("true", 4, &error);
    assert(val != NULL);
    assert(kjson_is_boolean(val));
    assert(kjson_get_boolean(val) == true);
    kjson_free(val);
    
    /* Test parsing number */
    val = kjson_parse("42.5", 4, &error);
    assert(val != NULL);
    assert(kjson_is_number(val));
    assert(kjson_get_number(val) == 42.5);
    kjson_free(val);
    
    /* Test parsing string */
    val = kjson_parse("\"hello world\"", 13, &error);
    assert(val != NULL);
    assert(kjson_is_string(val));
    assert(strcmp(kjson_get_string(val), "hello world") == 0);
    kjson_free(val);
    
    printf("  ✓ Basic types test passed\n");
}

void test_extended_types(void)
{
    printf("Testing extended types...\n");
    
    kjson_error error;
    
    /* Test BigInt */
    kjson_value *val = kjson_parse("123456789012345678901234567890n", 31, &error);
    assert(val != NULL);
    assert(kjson_is_bigint(val));
    const kjson_bigint *bigint = kjson_get_bigint(val);
    assert(bigint != NULL);
    assert(strcmp(bigint->digits, "123456789012345678901234567890") == 0);
    assert(bigint->negative == false);
    kjson_free(val);
    
    /* Test Decimal128 */
    val = kjson_parse("99.99m", 6, &error);
    assert(val != NULL);
    assert(kjson_is_decimal128(val));
    const kjson_decimal128 *decimal = kjson_get_decimal128(val);
    assert(decimal != NULL);
    assert(strcmp(decimal->digits, "99.99") == 0);
    assert(decimal->negative == false);
    kjson_free(val);
    
    /* Test UUID */
    val = kjson_parse("550e8400-e29b-41d4-a716-446655440000", 36, &error);
    assert(val != NULL);
    assert(kjson_is_uuid(val));
    kjson_free(val);
    
    /* Test Date */
    val = kjson_parse("2025-01-10T12:00:00Z", 20, &error);
    assert(val != NULL);
    assert(kjson_is_date(val));
    kjson_free(val);
    
    printf("  ✓ Extended types test passed\n");
}

void test_json5_features(void)
{
    printf("Testing JSON5 features...\n");
    
    kjson_error error;
    
    /* Test unquoted keys */
    const char *json5 = "{ name: \"Alice\", age: 30 }";
    kjson_value *val = kjson_parse(json5, strlen(json5), &error);
    assert(val != NULL);
    assert(kjson_is_object(val));
    assert(kjson_object_size(val) == 2);
    
    kjson_value *name = kjson_object_get(val, "name");
    assert(name != NULL);
    assert(kjson_is_string(name));
    assert(strcmp(kjson_get_string(name), "Alice") == 0);
    
    kjson_free(val);
    
    /* Test comments */
    const char *with_comments = "{\n  // This is a comment\n  value: 42\n}";
    val = kjson_parse(with_comments, strlen(with_comments), &error);
    assert(val != NULL);
    assert(kjson_is_object(val));
    kjson_free(val);
    
    /* Test trailing commas */
    const char *trailing = "[1, 2, 3,]";
    val = kjson_parse(trailing, strlen(trailing), &error);
    assert(val != NULL);
    assert(kjson_is_array(val));
    assert(kjson_array_size(val) == 3);
    kjson_free(val);
    
    printf("  ✓ JSON5 features test passed\n");
}

void test_stringify(void)
{
    printf("Testing stringify...\n");
    
    /* Create a complex object */
    kjson_value *obj = kjson_create_object();
    assert(obj != NULL);
    
    /* Add various types */
    kjson_object_set(obj, "name", kjson_create_string("Test", 4));
    kjson_object_set(obj, "count", kjson_create_number(42));
    kjson_object_set(obj, "active", kjson_create_boolean(true));
    kjson_object_set(obj, "bignum", kjson_create_bigint("999999999999999999", false));
    
    /* Stringify */
    char *json = kjson_stringify(obj);
    assert(json != NULL);
    printf("  Stringified: %s\n", json);
    
    /* Parse it back */
    kjson_error error;
    kjson_value *parsed = kjson_parse(json, strlen(json), &error);
    assert(parsed != NULL);
    assert(kjson_is_object(parsed));
    
    free(json);
    kjson_free(parsed);
    kjson_free(obj);
    
    printf("  ✓ Stringify test passed\n");
}

void test_binary_format(void)
{
    printf("Testing binary format...\n");
    
    /* Create a value */
    kjson_value *original = kjson_create_object();
    kjson_object_set(original, "test", kjson_create_string("binary", 6));
    kjson_object_set(original, "number", kjson_create_number(123.45));
    
    /* Encode to binary */
    size_t binary_size;
    uint8_t *binary = kjson_encode_binary(original, &binary_size);
    assert(binary != NULL);
    assert(binary_size > 0);
    
    printf("  Binary size: %zu bytes\n", binary_size);
    
    /* Decode from binary */
    kjson_error error;
    kjson_value *decoded = kjson_decode_binary(binary, binary_size, &error);
    assert(decoded != NULL);
    assert(kjson_is_object(decoded));
    
    /* Verify contents */
    kjson_value *test = kjson_object_get(decoded, "test");
    assert(test != NULL);
    assert(kjson_is_string(test));
    assert(strcmp(kjson_get_string(test), "binary") == 0);
    
    free(binary);
    kjson_free(original);
    kjson_free(decoded);
    
    printf("  ✓ Binary format test passed\n");
}

void test_arrays_and_objects(void)
{
    printf("Testing arrays and objects...\n");
    
    /* Create array */
    kjson_value *arr = kjson_create_array();
    assert(arr != NULL);
    
    /* Add elements */
    kjson_array_append(arr, kjson_create_number(1));
    kjson_array_append(arr, kjson_create_number(2));
    kjson_array_append(arr, kjson_create_number(3));
    
    assert(kjson_array_size(arr) == 3);
    
    kjson_value *elem = kjson_array_get(arr, 1);
    assert(elem != NULL);
    assert(kjson_is_number(elem));
    assert(kjson_get_number(elem) == 2);
    
    /* Stringify array */
    char *json = kjson_stringify(arr);
    assert(json != NULL);
    assert(strcmp(json, "[1, 2, 3]") == 0);
    
    free(json);
    kjson_free(arr);
    
    printf("  ✓ Arrays and objects test passed\n");
}

int main(void)
{
    printf("kJSON C Library Test Suite\n");
    printf("==========================\n\n");
    
    test_basic_types();
    test_extended_types();
    test_json5_features();
    test_stringify();
    test_binary_format();
    test_arrays_and_objects();
    
    printf("\nAll tests passed! ✓\n");
    return 0;
}