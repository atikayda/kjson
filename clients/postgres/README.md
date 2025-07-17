# kJSON PostgreSQL Extension

Native PostgreSQL type for storing and querying kJSON data with extended type support and nanosecond-precision temporal types.

## Overview

This C extension provides a native `kjson` type for PostgreSQL that:
- **Stores data in binary format** (kJSONB) for efficiency
- **Accepts and returns kJSON text** for easy interaction
- **Preserves extended types** like BigInt, Decimal128, UUID, Instant, and Duration
- **Provides native kInstant and kDuration types** with nanosecond precision
- **Unified temporal system** - kjson temporal types ARE kInstant/kDuration (zero-copy)
- **Integrates with PostgreSQL** operators and functions
- **Supports JSON5 syntax** including comments and unquoted keys

## Features

- Native kJSON type with binary storage
- Full kJSON text format support for input/output
- Extended type support:
  - BigInt with `n` suffix (arbitrary precision)
  - Decimal128 with `m` suffix (34 digits precision)
  - UUID (unquoted format)
  - Instant (nanosecond-precision timestamps)
  - Duration (ISO 8601 time spans)
- Native high-precision temporal types:
  - `kInstant` - Nanosecond-precision timestamps
  - `kDuration` - ISO 8601 durations with nanosecond precision
- JSON5 syntax support (comments, unquoted keys, trailing commas)
- Casting between kjson, json, and jsonb types
- Extraction operators (`->`, `->>`, `#>`, `#>>`)
- Path-based extraction functions with type preservation
- Aggregate functions for building arrays and objects
- GIN index support for efficient queries

## Installation

```bash
# Build the extension
make
make install

# Run tests (optional)
make installcheck
```

Then in PostgreSQL:

```sql
CREATE EXTENSION kjson;
```

## Usage

### Basic Operations

```sql
-- Create a table with kjson column
CREATE TABLE data (
    id serial PRIMARY KEY,
    doc kjson
);

-- Insert kJSON data (with extended types)
INSERT INTO data (doc) VALUES ('{
    id: 123456789012345678901234567890n,
    price: 99.99m,
    uuid: a0eebc99-9c0b-4ef8-bb6d-6bb9bd380a11,
    created: 2025-01-10T12:00:00Z,
    tags: ["json5", "extended"],
    // This is a comment
}');

-- Query returns kJSON text format
SELECT doc FROM data;
-- Output: {id: 123456789012345678901234567890n, price: 99.99m, ...}

-- Extract fields
SELECT doc->'id' FROM data;
-- Returns: 123456789012345678901234567890n

SELECT doc->>'tags' FROM data;
-- Returns: ["json5","extended"]
```

### Type Casting

```sql
-- Cast to standard JSON (loses extended types)
SELECT doc::json FROM data;
-- BigInt becomes string, Decimal128 becomes number, etc.

-- Cast from JSON to kjson
SELECT '{"name": "test"}'::json::kjson;

-- Cast from text to kjson
SELECT '{ unquoted: "keys", trailing: "commas", }'::kjson;
```

### Binary Format (COPY BINARY)

```sql
-- Export to binary format
COPY data TO '/tmp/data.bin' (FORMAT binary);

-- Import from binary format
COPY data FROM '/tmp/data.bin' (FORMAT binary);
```

## Type Mapping

| kJSON Type | PostgreSQL Type | Extraction Function |
|------------|----------------|-------------------|
| Number | Preserved in binary | Direct access via `->`/`->>` |
| BigInt (n suffix) | Preserved in binary | `kjson_extract_numeric()` |
| Decimal128 (m suffix) | Preserved in binary | `kjson_extract_numeric()` |
| String | TEXT | Direct access via `->`/`->>` |
| Boolean | BOOLEAN | Direct access via `->`/`->>` |
| Null | NULL | Direct access |
| UUID | Preserved in binary | `kjson_extract_uuid()` |
| Instant | kInstant (unified type) | `kjson_extract_kinstant()` |
| Duration | kDuration (unified type) | `kjson_extract_kduration()` |
| Array | Array in binary | Access with `->` or `kjson_array_elements()` |
| Object | Object in binary | Access with `->` or `kjson_object_keys()` |

## Functions and Operators

### Operators
- `->` - Extract object field or array element as kjson
- `->>` - Extract object field or array element as text
- `#>` - Extract kjson at path
- `#>>` - Extract text at path
- `@>` - Contains operator
- `<@` - Contained by operator
- `?` - Key exists
- `?|` - Any keys exist
- `?&` - All keys exist
- `=`, `<>`, `<`, `<=`, `>`, `>=` - Comparison operators

### Extraction Functions
- `kjson_extract_path(kjson, VARIADIC text[]) → kjson` - Extract value at path
- `kjson_extract_path_text(kjson, VARIADIC text[]) → text` - Extract text at path
- `kjson_extract_numeric(kjson, VARIADIC text[]) → numeric` - Extract numeric value
- `kjson_extract_uuid(kjson, VARIADIC text[]) → uuid` - Extract UUID value
- `kjson_extract_kinstant(kjson, VARIADIC text[]) → kInstant` - Extract instant with full precision
- `kjson_extract_kduration(kjson, VARIADIC text[]) → kDuration` - Extract duration
- `kjson_extract_instant(kjson, VARIADIC text[]) → timestamptz` - Extract as PostgreSQL timestamp

### Temporal Functions
- `k_now() → kInstant` - Current timestamp with nanosecond precision
- `kinstant_now() → kInstant` - Current timestamp (same as k_now)
- `kjson_build_instant(kInstant) → kjson` - Convert kInstant to kjson
- `kjson_build_duration(kDuration) → kjson` - Convert kDuration to kjson

### Utility Functions
- `kjson_pretty(kjson, indent int) → text` - Pretty print with indentation
- `kjson_compact(kjson) → text` - Compact format (no spaces)
- `kjson_typeof(kjson) → text` - Get type of kjson value
- `kjson_strip_nulls(kjson) → kjson` - Remove null values
- `kjson_array_length(kjson) → integer` - Get array length
- `kjson_array_elements(kjson) → SETOF kjson` - Expand array to rows
- `kjson_array_elements_text(kjson) → SETOF text` - Expand array to text rows
- `kjson_object_keys(kjson) → SETOF text` - Get object keys

### Builder Functions
- `kjson_build_object(VARIADIC "any") → kjson` - Build object from key/value pairs
- `kjson_build_array(VARIADIC "any") → kjson` - Build array from values
- `row_to_kjson(record) → kjson` - Convert table row to kjson

### Aggregate Functions
- `kjson_agg(anyelement) → kjson` - Aggregate values into array
- `kjson_object_agg(text, anyelement) → kjson` - Aggregate key/value pairs into object

## Examples

### Working with Extended Types

```sql
-- Store financial data with precise types
CREATE TABLE transactions (
    id serial PRIMARY KEY,
    data kjson
);

INSERT INTO transactions (data) VALUES 
('{
    transactionId: 550e8400-e29b-41d4-a716-446655440000,
    amount: 99999.99m,
    accountNumber: 1234567890123456789n,
    timestamp: 2025-01-15T10:30:00.123456789Z,
    timeout: PT5M,
    metadata: {
        source: "API",
        version: 2
    }
}');

-- Extract specific types
SELECT 
    kjson_extract_uuid(data, 'transactionId') as txn_id,
    kjson_extract_numeric(data, 'amount') as amount,
    kjson_extract_numeric(data, 'accountNumber') as account,
    kjson_extract_kinstant(data, 'timestamp') as when,
    kjson_extract_kduration(data, 'timeout') as timeout
FROM transactions;

-- Pretty print
SELECT kjson_pretty(data, 4) FROM transactions;
```

### JSON5 Features

```sql
-- kJSON supports JSON5 syntax
INSERT INTO transactions (data) VALUES ('
{
    // Transaction details
    id: 123n,
    status: "pending",
    tags: [
        "high-priority",
        "international",
    ], // trailing comma allowed
    details: {
        currency: \'USD\', // single quotes
        notes: "Wire transfer", 
    }
}
');
```

### Operators and Navigation

```sql
-- Navigate nested structures
SELECT 
    data->'metadata'->>'source' as source,
    data->'tags'->0 as first_tag,
    kjson_typeof(data->'amount') as amount_type
FROM transactions;

-- Aggregation
SELECT kjson_agg(data->'id') as all_ids
FROM transactions;

SELECT kjson_object_agg(
    data->>'id', 
    data->'amount'
) as id_to_amount_map
FROM transactions;
```

## Performance Considerations

- **Binary Storage**: kJSONB format is compact and efficient
- **Parsing Overhead**: Only incurred on input/output operations
- **Direct Binary Operations**: Operators work on binary format without parsing
- **Type Preservation**: No conversion overhead for extended types
- **Index Support**: GIN indexes planned for future versions

## Differences from JSONB

1. **Extended Types**: Native support for BigInt, Decimal128, UUID, Instant, Duration
2. **Text Format**: Human-readable kJSON with type suffixes  
3. **JSON5 Syntax**: Comments, unquoted keys, trailing commas
4. **Type Preservation**: Exact type information maintained
5. **Compatibility**: Can cast to/from standard JSON/JSONB

## Development

### Building from Source

Requirements:
- PostgreSQL 12+ development headers  
- C compiler (gcc/clang)
- GNU Make

```bash
# Clone the repository
git clone https://github.com/yourusername/kjson
cd kjson/clients/postgres

# Build
make

# Install
make install

# Run tests
make installcheck
```

### Project Structure

```
postgres/
├── Makefile                # Build configuration
├── kjson.control          # Extension control file
├── sql/
│   ├── kjson--1.0.0.sql   # Initial SQL definitions
│   ├── kjson--1.0.1.sql   # Current SQL definitions
│   └── kjson--1.0.0--1.0.1.sql  # Update script
├── src/
│   ├── kjson_internal.h   # Internal type definitions
│   ├── kjson_io.c         # I/O functions
│   ├── kjson_casts.c      # Cast functions
│   ├── kjson_ops.c        # Operators
│   └── kjson_utils.c      # Utility functions
└── test/
    └── sql/               # Test cases
```

## Future Enhancements

- GIN index support for efficient queries
- Path-based extraction operators
- JSON Schema validation
- Streaming parser for large documents
- Performance optimizations

## License

MIT License - see LICENSE file for details

## See Also

- [kJSON Specification](../../SPEC_TEXT.md)
- [kJSON Binary Format](../../SPEC_BINARY.md)
- [PostgreSQL Extensions Guide](https://www.postgresql.org/docs/current/extend-extensions.html)