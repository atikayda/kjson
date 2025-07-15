#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kjson.h"

int main() {
    // Test simple BigInt first
    printf("Testing simple BigInt parsing...\n");
    
    const char *simple_bigint = "123n";
    kjson_error error;
    kjson_value *val = kjson_parse(simple_bigint, strlen(simple_bigint), &error);
    
    if (val) {
        printf("Parsed successfully, type: %d\n", kjson_get_type(val));
        if (kjson_is_bigint(val)) {
            const kjson_bigint *bi = kjson_get_bigint(val);
            printf("BigInt value: %s, negative: %s\n", 
                   bi->digits ? bi->digits : "NULL", 
                   bi->negative ? "true" : "false");
        } else {
            printf("Not a BigInt, type: %d\n", kjson_get_type(val));
        }
        kjson_free(val);
    } else {
        printf("Parse failed: %s\n", kjson_error_string(error));
    }
    
    printf("\nTesting zero BigInt...\n");
    const char *zero_bigint = "0n";
    val = kjson_parse(zero_bigint, strlen(zero_bigint), &error);
    
    if (val) {
        printf("Parsed successfully, type: %d\n", kjson_get_type(val));
        if (kjson_is_bigint(val)) {
            const kjson_bigint *bi = kjson_get_bigint(val);
            printf("BigInt value: %s\n", bi->digits ? bi->digits : "NULL");
        }
        kjson_free(val);
    } else {
        printf("Parse failed: %s\n", kjson_error_string(error));
    }
    
    printf("\nTesting simple object with BigInt...\n");
    const char *obj_with_bigint = "{value: 123n}";
    val = kjson_parse(obj_with_bigint, strlen(obj_with_bigint), &error);
    
    if (val) {
        printf("Object parsed successfully\n");
        if (kjson_is_object(val)) {
            kjson_value *value = kjson_object_get(val, "value");
            if (value && kjson_is_bigint(value)) {
                printf("Found BigInt field\n");
            } else {
                printf("Field not found or not BigInt\n");
            }
        }
        kjson_free(val);
    } else {
        printf("Parse failed: %s\n", kjson_error_string(error));
    }
    
    printf("\nTesting simple decimal...\n");
    const char *simple_decimal = "123.45m";
    val = kjson_parse(simple_decimal, strlen(simple_decimal), &error);
    
    if (val) {
        printf("Parsed successfully, type: %d\n", kjson_get_type(val));
        if (kjson_is_decimal128(val)) {
            const kjson_decimal128 *dec = kjson_get_decimal128(val);
            printf("Decimal value: %s\n", dec->digits ? dec->digits : "NULL");
        } else {
            printf("Not a Decimal128, type: %d\n", kjson_get_type(val));
        }
        kjson_free(val);
    } else {
        printf("Parse failed: %s\n", kjson_error_string(error));
    }
    
    return 0;
}