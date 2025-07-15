# kJSON Multi-Language Clients

This directory contains kJSON parser and serializer implementations for various programming languages and platforms, ensuring cross-language interoperability.

## Overview

kJSON is a superset of JSON that adds support for:
- **BigInt** values with `n` suffix (e.g., `123n`)
- **UUID** objects as unquoted values (e.g., `550e8400-e29b-41d4-a716-446655440000`)
- **Decimal128** values with `m` suffix (e.g., `123.45m`)
- **Instant** objects as ISO timestamps (e.g., `2025-01-01T00:00:00.000Z`)
- **Duration** objects as ISO timestamps (e.g., `PT1H2M3S`)
- **Binary data** (kJSONB format only)
- **JSON5 features**: comments, unquoted keys, trailing commas

## Language Implementations

### Production Ready
- **JavaScript/TypeScript** (Deno) - Reference implementation in parent directory

### In Development
- **[PostgreSQL](./postgres/)** - PL/pgSQL functions and JSONB extensions
- **[Go](./golang/)** - Native Go implementation with struct tags
- **[Python](./python/)** - Pure Python and C extension implementations
- **[Rust](./rust/)** - Zero-copy parser with serde integration
- **[C](./c/)** - Low-level C implementation for embedded systems

## Interoperability Testing

Each implementation includes:
1. **Parser** - Read kJSON text format
2. **Serializer** - Write kJSON text format
3. **Binary codec** - Encode/decode kJSONB binary format
4. **Test suite** - Common test cases for cross-language validation

## Wire Format Specifications

### Text Format (kJSON)
See [SPEC_TEXT.md](./SPEC_TEXT.md) for the complete text format specification.

### Binary Format (kJSONB)
See [SPEC_BINARY.md](./SPEC_BINARY.md) for the complete binary format specification.

## Common Test Data

The `testdata/` directory contains standardized test files used by all implementations:
- `basic.kjson` - Basic types and structures
- `extended.kjson` - Extended types (UUID, Decimal128, BigInt, Date)
- `complex.kjson` - Nested structures and edge cases
- `invalid/` - Invalid inputs for error handling tests
- `binary/` - Binary format test files

## Implementation Guidelines

When implementing a kJSON client:

1. **Type Mapping**
   - Map kJSON types to appropriate native types
   - Preserve precision for BigInt and Decimal128
   - Handle UUID as a distinct type or specialized string
   - Parse dates to native date/time types

2. **Parser Requirements**
   - Support JSON5 syntax (comments, unquoted keys, etc.)
   - Detect type suffixes (`n` for BigInt, `m` for Decimal)
   - Parse unquoted UUIDs
   - Handle ISO date strings

3. **Serializer Requirements**
   - Output valid kJSON with appropriate type markers
   - Support pretty-printing with indentation
   - Quote keys when necessary (reserved words, special chars)
   - Preserve type information

4. **Binary Format**
   - Implement type-first encoding
   - Use variable-length integer encoding
   - Support all extended types
   - Maintain round-trip compatibility

## Contributing

When adding a new language implementation:

1. Create a directory under `clients/` with the language name
2. Include a language-specific README with:
   - Installation instructions
   - Usage examples
   - Type mapping documentation
   - Performance characteristics
3. Implement the test suite using common test data
4. Add CI/CD configuration for automated testing
5. Update this README with the new implementation

## License

All client implementations are released under the MIT License, same as the main kJSON project.