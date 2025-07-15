# kJSONB Binary Format Specification

Version 1.0

## Overview

kJSONB is a compact, self-describing binary format for kJSON data that preserves all type information while providing efficient encoding and fast parsing.

## Design Principles

1. **Type-first encoding** - Type byte precedes value for fast type detection
2. **Self-describing** - No schema required
3. **Variable-length integers** - Space-efficient number encoding
4. **Little-endian** - Consistent byte order
5. **Streaming-friendly** - Can be parsed without buffering entire document

## Type Bytes

| Type | Byte | Description |
|------|------|-------------|
| NULL | 0x00 | Null value |
| FALSE | 0x01 | Boolean false |
| TRUE | 0x02 | Boolean true |
| INT8 | 0x10 | 8-bit signed integer |
| INT16 | 0x11 | 16-bit signed integer |
| INT32 | 0x12 | 32-bit signed integer |
| INT64 | 0x13 | 64-bit signed integer |
| UINT64 | 0x14 | 64-bit unsigned integer |
| FLOAT32 | 0x15 | 32-bit float (IEEE 754) |
| FLOAT64 | 0x16 | 64-bit float (IEEE 754) |
| BIGINT | 0x17 | Arbitrary precision integer |
| DECIMAL128 | 0x18 | 128-bit decimal |
| STRING | 0x20 | UTF-8 string |
| BINARY | 0x21 | Raw binary data |
| DATE | 0x30 | Date/time (milliseconds since epoch) |
| UUID | 0x31 | 16-byte UUID |
| ARRAY | 0x40 | Array of values |
| OBJECT | 0x41 | Object with string keys |
| UNDEFINED | 0xF0 | Undefined value |

## Encoding Rules

### Null and Boolean
```
NULL:      0x00
FALSE:     0x01
TRUE:      0x02
UNDEFINED: 0xF0
```

### Numbers

#### Small Integers
Choose the smallest representation:
```
INT8:  0x10 <int8>
INT16: 0x11 <int16_le>
INT32: 0x12 <int32_le>
INT64: 0x13 <int64_le>
```

#### Floating Point
```
FLOAT32: 0x15 <float32_le>
FLOAT64: 0x16 <float64_le>
```

Special values (NaN, Infinity) are encoded as NULL.

#### BigInt
```
BIGINT: 0x17 <varint:flags+length> <digits>
```
- Bit 0 of flags: 0=positive, 1=negative
- Remaining bits: length of digit string
- Digits: UTF-8 encoded decimal digits

#### Decimal128
```
DECIMAL128: 0x18 <varint:length> <string>
```
Currently stored as string representation. Future versions may use IEEE 754-2008 decimal128.

### Strings
```
STRING: 0x20 <varint:length> <utf8_bytes>
```

### Binary Data
```
BINARY: 0x21 <varint:length> <raw_bytes>
```

### Date
```
DATE: 0x30 <int64_le:milliseconds>
```
Milliseconds since Unix epoch (1970-01-01T00:00:00.000Z).

### UUID
```
UUID: 0x31 <16_bytes>
```
16 bytes in standard UUID byte order.

### Arrays
```
ARRAY: 0x40 <varint:count> <value>*
```
Count followed by that many values.

### Objects
```
OBJECT: 0x41 <varint:count> (<varint:key_length> <key_bytes> <value>)*
```
Count of key-value pairs, each key is UTF-8 string without type byte.

## Variable-Length Integer Encoding

Varints use LEB128 encoding:
- Bits 0-6: data
- Bit 7: continuation bit (1=more bytes follow)

Examples:
- 0-127: 1 byte
- 128-16383: 2 bytes
- 16384-2097151: 3 bytes

```
127:   0x7F
128:   0x80 0x01
300:   0xAC 0x02
```

## Examples

### Simple Values
```
null:          00
true:          02
false:         01
undefined:     F0
```

### Numbers
```
42:            10 2A                    // INT8 42
-1000:         11 18 FC                 // INT16 -1000
1000000:       12 40 42 0F 00          // INT32
3.14159:       16 6E 86 1B F0 F9 21 09 40  // FLOAT64
123n:          17 06 31 32 33          // BIGINT "123"
45.67m:        18 05 34 35 2E 36 37    // DECIMAL128 "45.67"
```

### Strings
```
"hello":       20 05 68 65 6C 6C 6F
"":            20 00
"😀":          20 04 F0 9F 98 80
```

### Complex Types
```
UUID 550e8400-e29b-41d4-a716-446655440000:
31 55 0E 84 00 E2 9B 41 D4 A7 16 44 66 55 44 00 00

Date 2025-01-01T00:00:00.000Z:
30 00 C0 5C 8F 93 01 00 00

[1, 2, 3]:
40 03 10 01 10 02 10 03

{"a": 1, "b": 2}:
41 02 01 61 10 01 01 62 10 02
```

## Size Optimization

The format optimizes for common cases:
- Small integers (0-127) use 2 bytes total
- Short strings (<128 chars) use 2 bytes overhead
- Common types (null, true, false) use 1 byte

## Streaming

The format supports streaming in both directions:

### Streaming Parse
```c
while (has_more_data()) {
    uint8_t type = read_byte();
    switch (type) {
        case TYPE_ARRAY:
            uint32_t count = read_varint();
            for (uint32_t i = 0; i < count; i++) {
                parse_value();
            }
            break;
        // ... other types
    }
}
```

### Streaming Write
Values can be written sequentially without buffering.

## Versioning

The format is designed for forward compatibility:
- Unknown type bytes should cause parse errors
- New types can be added with unused type bytes
- Type byte ranges are reserved for future use

## Comparison with Other Formats

| Feature | kJSONB | MessagePack | CBOR | BSON |
|---------|---------|-------------|------|------|
| Self-describing | ✓ | ✓ | ✓ | ✓ |
| Schema-less | ✓ | ✓ | ✓ | ✓ |
| BigInt | ✓ | ✗ | ✓ | ✗ |
| Decimal128 | ✓ | ✗ | ✓ | ✓ |
| UUID native | ✓ | ✗ | ✓ | ✓ |
| Streaming | ✓ | ✓ | ✓ | ✗ |
| Type-first | ✓ | ✓ | ✓ | ✗ |

## Implementation Notes

1. **Buffer Management**: Implementations should support both fixed and growing buffers
2. **Validation**: Type bytes should be validated before parsing values
3. **Overflow**: Integer overflow should be detected and handled appropriately
4. **Memory Limits**: Implementations should enforce reasonable size limits
5. **Security**: Malformed input should not cause crashes or memory corruption

## Test Vectors

See `testdata/binary/` for reference test files with known encodings.