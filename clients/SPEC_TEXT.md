# kJSON Text Format Specification

Version 1.0

## Overview

kJSON (Kind JSON) is a superset of JSON that adds extended type support and JSON5 syntax features while maintaining backward compatibility with standard JSON.

## Syntax

### Basic Values

#### Null
```
null
```

#### Boolean
```
true
false
```

#### Numbers
Standard JSON numbers:
```
42
-3.14159
6.022e23
```

#### Strings
Double or single quoted:
```
"Hello, World!"
'Hello, World!'
```

Escape sequences:
- `\"` or `\'` - Quote
- `\\` - Backslash
- `\/` - Forward slash
- `\b` - Backspace
- `\f` - Form feed
- `\n` - Line feed
- `\r` - Carriage return
- `\t` - Tab
- `\uXXXX` - Unicode character

### Extended Types

#### BigInt
Integer values with `n` suffix:
```
123n
-456789012345678901234567890n
0n
```

#### Decimal128
Decimal values with `m` suffix (34 significant digits):
```
123.45m
-0.0000000000000000000000000000000001m
99999999999999999999999999999999.99m
```

#### UUID
Unquoted UUID values in standard format:
```
550e8400-e29b-41d4-a716-446655440000
01234567-89ab-cdef-0123-456789abcdef
```

Format: 8-4-4-4-12 hexadecimal digits

#### Instant
ISO 8601 format timestamps with nanosecond precision in Zulu time:
```
2025-01-01T00:00:00.000Z
2025-01-01T00:00:00.123456789Z
2025-12-31T23:59:59.999999999Z
```

Note: Timezones are converted to Zulu time (UTC) during parsing. If a timestamp with a timezone is provided, it will be converted to UTC:
```
2025-01-15T10:30:00+05:30  →  2025-01-15T05:00:00Z
```

#### Duration
ISO 8601 format durations with nanosecond precision:
```
PT1H2M3S
P1DT2H3M4S
PT0.000000001S
PT1.123456789S
```

### Objects

Standard syntax:
```json
{"key": "value", "number": 42}
```

JSON5 features:
```javascript
{
  // Unquoted keys
  name: "Alice",
  age: 30,
  
  // Trailing comma
  active: true,
}
```

### Arrays

Standard syntax:
```json
[1, 2, 3]
```

With trailing comma:
```javascript
[
  "first",
  "second",
  "third",  // trailing comma allowed
]
```

### Comments

Single-line:
```javascript
// This is a comment
{
  name: "Bob", // inline comment
}
```

Multi-line:
```javascript
/*
 * Multi-line comment
 * with multiple lines
 */
{
  /* inline multi-line */ value: 42
}
```

## JSON5 Features

### Unquoted Keys
Object keys that are valid identifiers don't need quotes:
```javascript
{
  name: "value",
  _private: true,
  $special: 123,
  camelCase: "works"
}
```

Keys must be quoted if they:
- Start with a digit
- Contain special characters (except `_` and `$`)
- Are reserved words (`true`, `false`, `null`, `undefined`)

### Trailing Commas
Allowed in objects and arrays:
```javascript
{
  a: 1,
  b: 2,
}

[1, 2, 3,]
```

### Single Quotes and Backticks
Strings can use single quotes or backticks:
```javascript
{
  'single': 'quoted',
  "double": "quoted",
  `backtick`: `quoted`
}
```

### Line Continuations
Long strings can span multiple lines:
```kjson
"This is a very long string that
continues on the next line"
```

## Type Detection Rules

The parser determines types based on the following rules:

1. **Quoted values** are always strings
2. **Unquoted values** are checked in order:
   - `true`, `false` → Boolean
   - `null` → Null
   - `undefined` → Undefined (parsing only)
   - UUID pattern → UUID
   - ISO date pattern → Instant
   - ISO duration pattern → Duration
   - Number with `n` suffix → BigInt
   - Number with `m` suffix → Decimal128
   - Valid number → Number
   - Otherwise → Identifier (for object keys)

## Serialization Rules

When serializing to kJSON:

1. **BigInt**: Add `n` suffix
2. **Decimal128**: Add `m` suffix
3. **UUID**: Output unquoted
4. **Instant, Duration or Date**: Output as ISO 8601 string
5. **Keys**: Quote if necessary (special chars, reserved words)
6. **Strings**: Use double quotes by default

## Examples

### Complete Example
```javascript
{
  // User profile
  id: 550e8400-e29b-41d4-a716-446655440000,
  username: "alice_wonder",
  displayName: 'Alice Wonder',
  
  // Financial data
  balance: 12345.67m,
  totalTransactions: 98765432109876543210n,
  
  // Timestamps
  created: 2025-01-01T00:00:00.000Z,
  lastLogin: 2025-01-15T10:30:00.000Z,
  
  // Nested data
  preferences: {
    theme: "dark",
    notifications: {
      email: true,
      sms: false,
    },
  },
  
  // Arrays
  tags: [
    "premium",
    "verified",
    "early-adopter",
  ],
}
```

### Edge Cases

#### Numbers vs Strings
```javascript
{
  number: 123,        // Number
  bigint: 123n,       // BigInt
  decimal: 123m,      // Decimal128
  string: "123",      // String
  stringN: "123n",    // String (quoted)
  stringM: "123m",    // String (quoted)
}
```

#### UUID vs String
```javascript
{
  uuid: 550e8400-e29b-41d4-a716-446655440000,  // UUID object
  string: "550e8400-e29b-41d4-a716-446655440000", // String
}
```

#### Reserved Keys
```javascript
{
  "true": 1,      // Must be quoted
  "false": 2,     // Must be quoted
  "null": 3,      // Must be quoted
  "undefined": 4, // Must be quoted
  regular: 5,     // Can be unquoted
}
```

## Conformance

A conforming kJSON parser MUST:
1. Accept all valid JSON documents
2. Support the extended types (BigInt, Decimal128, UUID, Instant, Duration)
3. Support JSON5 syntax features (comments, unquoted keys, trailing commas)
4. Preserve type information during round-trip parsing and serialization

A conforming kJSON serializer MUST:
1. Output valid kJSON text
2. Use appropriate type suffixes for extended types
3. Quote object keys when necessary
4. Preserve precision for BigInt and Decimal128 values