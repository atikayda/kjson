# kJSON C Library

A comprehensive C implementation of the kJSON format with extended type support.

## Overview

This implementation provides:
- **Full kJSON parser and stringifier** with DOM-style API
- **Extended type support** for BigInt, Decimal128, UUID, and Date
- **Binary format (kJSONB)** encoding and decoding
- **JSON5 syntax support** including comments and unquoted keys
- **Memory management flexibility** with custom allocators
- **Zero dependencies** beyond standard C library

## Features

- Complete kJSON text format parsing and stringification
- Extended type support:
  - BigInt with arbitrary precision (stored as strings)
  - Decimal128 with configurable precision
  - UUID automatic detection and parsing
  - ISO 8601 date/time parsing
- JSON5 syntax features:
  - Single and multi-line comments
  - Unquoted object keys
  - Trailing commas
  - Single quotes for strings
- Binary format (kJSONB) for efficient storage
- Configurable memory allocator
- Comprehensive error reporting

## Building

```bash
# Build static and shared libraries
make

# Run tests
make test

# Install to system (default: /usr/local)
sudo make install

# Custom prefix
make install PREFIX=/opt/kjson
```

### Library Files
- `libkjson.a` - Static library
- `libkjson.so` - Shared library
- `kjson.h` - Header file

## Usage

### Basic Parsing

```c
#include <kjson.h>
#include <stdio.h>
#include <string.h>

int main() {
    const char *text = "{name: \"Alice\", age: 30, balance: 123.45m}";
    
    // Parse kJSON text
    kjson_error error;
    kjson_value *value = kjson_parse(text, strlen(text), &error);
    if (value == NULL) {
        printf("Parse error: %s\n", kjson_error_string(error));
        return 1;
    }
    
    // Access object fields
    kjson_value *name = kjson_object_get(value, "name");
    if (name && kjson_is_string(name)) {
        printf("Name: %s\n", kjson_get_string(name));
    }
    
    kjson_value *age = kjson_object_get(value, "age");
    if (age && kjson_is_number(age)) {
        printf("Age: %d\n", (int)kjson_get_number(age));
    }
    
    kjson_value *balance = kjson_object_get(value, "balance");
    if (balance && kjson_is_decimal128(balance)) {
        const kjson_decimal128 *dec = kjson_get_decimal128(balance);
        printf("Balance: %s%s\n", dec->negative ? "-" : "", dec->digits);
    }
    
    // Stringify back
    char *output = kjson_stringify(value);
    printf("Output: %s\n", output);
    
    // Cleanup
    free(output);
    kjson_free(value);
    
    return 0;
}
```

### Extended Types

```c
// Parse and handle extended types
const char *text = "{
    id: 123456789012345678901234567890n,
    price: 99.99m,
    uuid: 550e8400-e29b-41d4-a716-446655440000,
    created: 2025-01-10T12:00:00Z
}";

kjson_error error;
kjson_value *obj = kjson_parse(text, strlen(text), &error);

// BigInt
kjson_value *id = kjson_object_get(obj, "id");
if (kjson_is_bigint(id)) {
    const kjson_bigint *bi = kjson_get_bigint(id);
    printf("BigInt: %s%s\n", bi->negative ? "-" : "", bi->digits);
}

// UUID
kjson_value *uuid = kjson_object_get(obj, "uuid");
if (kjson_is_uuid(uuid)) {
    const kjson_uuid *u = kjson_get_uuid(uuid);
    // UUID bytes available in u->bytes[16]
}

// Date
kjson_value *created = kjson_object_get(obj, "created");
if (kjson_is_date(created)) {
    const kjson_date *d = kjson_get_date(created);
    printf("Timestamp: %lld ms\n", (long long)d->milliseconds);
}
```

### Binary Format

```c
// Encode to binary
size_t binary_size;
uint8_t *binary = kjson_encode_binary(value, &binary_size);
if (binary) {
    // Write to file or send over network
    FILE *f = fopen("data.kjsonb", "wb");
    fwrite(binary, 1, binary_size, f);
    fclose(f);
    free(binary);
}

// Decode from binary
kjson_value *decoded = kjson_decode_binary(binary_data, binary_size, &error);
if (decoded) {
    // Use decoded value
    kjson_free(decoded);
}
```

### Creating Values

```c
// Build a complex structure
kjson_value *obj = kjson_create_object();
kjson_object_set(obj, "name", kjson_create_string("Product", 7));
kjson_object_set(obj, "price", kjson_create_decimal128("19.99", 0, false));
kjson_object_set(obj, "stock", kjson_create_bigint("999999999999", false));

kjson_value *tags = kjson_create_array();
kjson_array_append(tags, kjson_create_string("new", 3));
kjson_array_append(tags, kjson_create_string("sale", 4));
kjson_object_set(obj, "tags", tags);

// Pretty print
kjson_write_options opts = {
    .pretty = true,
    .indent = 2,
    .quote_keys = false
};
char *json = kjson_stringify_ex(obj, &opts);
printf("%s\n", json);

free(json);
kjson_free(obj);
```

### Custom Options

```c
// Configure parser options
kjson_options opts = {
    .allow_comments = true,
    .allow_trailing_commas = true,
    .allow_unquoted_keys = true,
    .parse_dates = true,
    .strict_numbers = false,
    .max_depth = 1000,
    .max_string_length = 1024 * 1024 * 1024,  // 1GB
    .allocator = my_malloc,
    .deallocator = my_free,
    .allocator_ctx = my_context
};

kjson_value *value = kjson_parse_ex(text, length, &opts, &error);
```

## API Reference

### Core Functions

```c
// Parsing
kjson_value *kjson_parse(const char *text, size_t length, kjson_error *error);
kjson_value *kjson_parse_ex(const char *text, size_t length, 
                            const kjson_options *options, kjson_error *error);

// Stringifying
char *kjson_stringify(const kjson_value *value);
char *kjson_stringify_ex(const kjson_value *value, const kjson_write_options *options);

// Memory
void kjson_free(kjson_value *value);

// Binary format
uint8_t *kjson_encode_binary(const kjson_value *value, size_t *size);
kjson_value *kjson_decode_binary(const uint8_t *data, size_t size, kjson_error *error);
```

### Type Checking

```c
kjson_type kjson_get_type(const kjson_value *value);
bool kjson_is_null(const kjson_value *value);
bool kjson_is_boolean(const kjson_value *value);
bool kjson_is_number(const kjson_value *value);
bool kjson_is_string(const kjson_value *value);
bool kjson_is_array(const kjson_value *value);
bool kjson_is_object(const kjson_value *value);
bool kjson_is_bigint(const kjson_value *value);
bool kjson_is_decimal128(const kjson_value *value);
bool kjson_is_uuid(const kjson_value *value);
bool kjson_is_date(const kjson_value *value);
```

### Value Access

```c
// Basic types
bool kjson_get_boolean(const kjson_value *value);
double kjson_get_number(const kjson_value *value);
const char *kjson_get_string(const kjson_value *value);

// Extended types
const kjson_bigint *kjson_get_bigint(const kjson_value *value);
const kjson_decimal128 *kjson_get_decimal128(const kjson_value *value);
const kjson_uuid *kjson_get_uuid(const kjson_value *value);
const kjson_date *kjson_get_date(const kjson_value *value);

// Containers
size_t kjson_array_size(const kjson_value *array);
kjson_value *kjson_array_get(const kjson_value *array, size_t index);
size_t kjson_object_size(const kjson_value *object);
kjson_value *kjson_object_get(const kjson_value *object, const char *key);
bool kjson_object_has(const kjson_value *object, const char *key);
```

### Value Creation

```c
kjson_value *kjson_create_null(void);
kjson_value *kjson_create_boolean(bool value);
kjson_value *kjson_create_number(double value);
kjson_value *kjson_create_string(const char *str, size_t len);
kjson_value *kjson_create_bigint(const char *digits, bool negative);
kjson_value *kjson_create_decimal128(const char *digits, int32_t exp, bool negative);
kjson_value *kjson_create_uuid(const kjson_uuid *uuid);
kjson_value *kjson_create_date(int64_t ms, int16_t tz_offset);
kjson_value *kjson_create_array(void);
kjson_value *kjson_create_object(void);

// Container operations
kjson_error kjson_array_append(kjson_value *array, kjson_value *value);
kjson_error kjson_object_set(kjson_value *object, const char *key, kjson_value *value);
```

## Type Definitions

```c
// Error codes
typedef enum {
    KJSON_OK = 0,
    KJSON_ERROR_MEMORY,
    KJSON_ERROR_SYNTAX,
    KJSON_ERROR_UNEXPECTED_TOKEN,
    KJSON_ERROR_INVALID_NUMBER,
    KJSON_ERROR_INVALID_STRING,
    KJSON_ERROR_INVALID_UUID,
    KJSON_ERROR_INVALID_DATE,
    KJSON_ERROR_INVALID_ESCAPE,
    KJSON_ERROR_DEPTH_EXCEEDED,
    KJSON_ERROR_SIZE_EXCEEDED,
    KJSON_ERROR_INVALID_UTF8,
    KJSON_ERROR_TRAILING_DATA,
    KJSON_ERROR_INCOMPLETE,
    KJSON_ERROR_UNSUPPORTED_TYPE,
    KJSON_ERROR_OVERFLOW,
    KJSON_ERROR_INVALID_BINARY
} kjson_error;

// Value types
typedef enum {
    KJSON_TYPE_NULL,
    KJSON_TYPE_BOOLEAN,
    KJSON_TYPE_NUMBER,
    KJSON_TYPE_BIGINT,
    KJSON_TYPE_DECIMAL128,
    KJSON_TYPE_STRING,
    KJSON_TYPE_UUID,
    KJSON_TYPE_DATE,
    KJSON_TYPE_ARRAY,
    KJSON_TYPE_OBJECT,
    KJSON_TYPE_UNDEFINED,
    KJSON_TYPE_BINARY
} kjson_type;

// Extended types
typedef struct {
    char *digits;
    bool negative;
} kjson_bigint;

typedef struct {
    char *digits;
    int32_t exponent;
    bool negative;
} kjson_decimal128;

typedef struct {
    uint8_t bytes[16];
} kjson_uuid;

typedef struct {
    int64_t milliseconds;
    int16_t tz_offset;
} kjson_date;
```

## Performance Considerations

- The parser allocates memory for the entire value tree
- Strings are copied (not referenced) for safety
- BigInt and Decimal128 store digits as strings
- Binary format is more compact and faster to parse
- Use custom allocators for embedded systems

## Thread Safety

The library functions are thread-safe when:
- Each thread uses its own parser instance
- Value trees are not shared between threads during modification
- Custom allocators are thread-safe

## Examples

See `test_kjson.c` for comprehensive usage examples.

## License

MIT License - see LICENSE file for details.

## See Also

- [kJSON Text Format Specification](../../SPEC_TEXT.md)
- [kJSON Binary Format Specification](../../SPEC_BINARY.md)
- [Main kJSON Library](../../README.md)