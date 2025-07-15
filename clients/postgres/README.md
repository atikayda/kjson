# kJSON PostgreSQL Extension

Native PostgreSQL type for storing and querying kJSON data with extended type support.

## Overview

This C extension provides a native `kjson` type for PostgreSQL that:
- **Stores data in binary format** (kJSONB) for efficiency
- **Accepts and returns kJSON text** for easy interaction
- **Preserves extended types** like BigInt, Decimal128, UUID, and Date
- **Integrates with PostgreSQL** operators and functions
- **Supports JSON5 syntax** including comments and unquoted keys

## Features

- Native kJSON type with binary storage
- Full kJSON text format support for input/output
- Extended type support:
  - BigInt with `n` suffix (arbitrary precision)
  - Decimal128 with `m` suffix (34 digits precision)
  - UUID (unquoted format)
  - Date (ISO 8601 timestamps)
- JSON5 syntax support (comments, unquoted keys, trailing commas)
- Casting between kjson, json, and jsonb types
- Extraction operators (`->`, `->>`)
- Utility functions for type inspection and formatting

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

| kJSON Type | PostgreSQL Type | Notes |
|------------|----------------|-------|
| Number | Preserved in binary | Standard JSON numbers |
| BigInt (n suffix) | Preserved in binary | Extract with kjson_extract_numeric() |
| Decimal128 (m suffix) | Preserved in binary | Extract with kjson_extract_numeric() |
| String | TEXT | UTF-8 |
| Boolean | BOOLEAN | |
| Null | NULL | |
| UUID | Preserved in binary | Extract with kjson_extract_uuid() |
| Date | Preserved in binary | Extract with kjson_extract_timestamp() |
| Array | Array in binary | Access with -> operator |
| Object | Object in binary | Access with -> operator |

## Functions and Operators

### Operators
- `->` - Extract object field or array element as kjson
- `->>` - Extract object field or array element as text
- `=` - Equality comparison
- `<>` - Inequality comparison

### Utility Functions
- `kjson_pretty(kjson, indent int) → text` - Pretty print with indentation
- `kjson_typeof(kjson) → text` - Get type of kjson value
- `kjson_extract_numeric(kjson, path...) → numeric` - Extract numeric value
- `kjson_extract_uuid(kjson, path...) → uuid` - Extract UUID value
- `kjson_extract_timestamp(kjson, path...) → timestamptz` - Extract timestamp

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
    timestamp: 2025-01-15T10:30:00Z,
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
    kjson_extract_timestamp(data, 'timestamp') as when
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

1. **Extended Types**: Native support for BigInt, Decimal128, UUID, Date
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
├── Makefile              # Build configuration
├── kjson.control         # Extension control file
├── sql/
│   └── kjson--1.0.0.sql  # SQL definitions
├── src/
│   ├── kjson.h           # Header file
│   ├── kjson_io.c        # I/O functions
│   ├── kjson_casts.c     # Cast functions
│   └── kjson_ops.c       # Operators
└── test/
    └── sql/              # Test cases
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