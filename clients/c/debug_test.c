#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kjson.h"

int main() {
    printf("Debugging UUID parsing...\n");
    
    const char *uuid_str = "550e8400-e29b-41d4-a716-446655440000";
    printf("Parsing UUID: %s\n", uuid_str);
    printf("Length: %zu\n", strlen(uuid_str));
    
    kjson_error error;
    kjson_value *val = kjson_parse(uuid_str, strlen(uuid_str), &error);
    
    if (val == NULL) {
        printf("Parse failed with error: %d (%s)\n", error, kjson_error_string(error));
        return 1;
    }
    
    printf("Parse succeeded\n");
    printf("Type: %d\n", kjson_get_type(val));
    printf("Is UUID: %s\n", kjson_is_uuid(val) ? "true" : "false");
    
    kjson_free(val);
    return 0;
}