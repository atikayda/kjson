# kJSON Rust Client

A Rust implementation of the kJSON (Kind JSON) specification with extended type support including BigInt, Decimal128, UUID, Instant, and Duration types.

## Features

- **BigInt Support** - Handle arbitrarily large integers (with `n` suffix)
- **Decimal128 Support** - High-precision decimal numbers (with `m` suffix)
- **UUID Support** - Native UUID parsing and generation (v4 and v7)
- **Instant Support** - Nanosecond-precision timestamps in Zulu time (UTC)
- **Duration Support** - ISO 8601 duration format with nanosecond precision
- **JSON5 Syntax** - Unquoted keys, trailing commas, comments
- **Serde Integration** - Works with existing Rust serialization ecosystem
- **Zero-Copy Parsing** - Efficient memory usage where possible

## Installation

Add this to your `Cargo.toml`:

```toml
[dependencies]
kjson = "0.1.0"
```

For derive macro support (coming soon):

```toml
[dependencies]
kjson = { version = "0.1.0", features = ["derive"] }
```

## Quick Start

```rust
use kjson::{parse, to_string, Value};
use kjson::types::{BigInt, Decimal128, Instant, Duration, uuid_v4};
use std::collections::HashMap;

fn main() -> kjson::Result<()> {
    // Parse kJSON with extended types
    let input = r#"{
        id: 550e8400-e29b-41d4-a716-446655440000,
        bigNumber: 123456789012345678901234567890n,
        price: 99.99m,
        created: 2025-01-10T12:00:00.123456789Z,
        timeout: PT1H30M,
        active: true,
        tags: ["new", "sale"]
    }"#;
    
    let value = parse(input)?;
    
    // Access values
    if let Value::Object(obj) = &value {
        if let Some(Value::BigInt(big)) = obj.get("bigNumber") {
            println!("BigInt: {}", big);
        }
    }
    
    // Serialize back to kJSON
    let output = to_string(&value)?;
    println!("{}", output);
    
    Ok(())
}
```

## Extended Types

### BigInt

For integers larger than what standard JSON numbers can handle:

```rust
use kjson::{parse, to_string, Value};
use kjson::types::BigInt;

// Parse BigInt
let value = parse("123456789012345678901234567890n")?;
match value {
    Value::BigInt(b) => println!("BigInt: {}", b),
    _ => {}
}

// Create BigInt
let big = BigInt::from_i64(123456789012345678);
let value = Value::BigInt(big);
let json = to_string(&value)?; // "123456789012345678n"
```

### Decimal128

For high-precision decimal arithmetic:

```rust
use kjson::types::Decimal128;

// Parse Decimal128
let value = parse("99.99m")?;

// Create Decimal128
let decimal = Decimal128::from_str("99.99")?;
let value = Value::Decimal128(decimal);
let json = to_string(&value)?; // "99.99m"
```

### UUID

Native UUID support with generation:

```rust
use kjson::types::{uuid_v4, uuid_v7};

// Generate UUIDs
let uuid4 = uuid_v4(); // Random UUID
let uuid7 = uuid_v7(); // Timestamp-based UUID

// Parse UUID
let value = parse("550e8400-e29b-41d4-a716-446655440000")?;
match value {
    Value::Uuid(u) => println!("UUID: {}", u),
    _ => {}
}
```

### Instant

Nanosecond-precision timestamps in Zulu time (UTC):

```rust
use kjson::types::Instant;
use chrono::Utc;

// Create Instant
let instant = Instant::now();
let value = Value::Instant(instant);
let json = to_string(&value)?; // "2025-01-10T12:00:00.123456789Z"

// Parse Instant
let value = parse("2025-01-10T12:00:00.123456789Z")?;
```

### Duration

ISO 8601 duration format with nanosecond precision:

```rust
use kjson::types::Duration;
use std::time::Duration as StdDuration;

// Create Duration
let duration = Duration::from_std(StdDuration::from_secs(3600));
let value = Value::Duration(duration);
let json = to_string(&value)?; // "PT1H"

// Parse Duration
let value = parse("PT2H30M")?;
```

## JSON5 Features

The parser supports JSON5 syntax for more readable configuration files:

```rust
let input = r#"{
    // Comments are supported
    name: "test",      // Unquoted keys
    values: [
        1,
        2,
        3,             // Trailing commas allowed
    ],
    /* Block comments too */
}"#;

let value = parse(input)?;
```

## Serde Integration

The library provides basic serde integration for converting between Rust types and kJSON:

```rust
use kjson::{from_str, to_string_pretty};
use serde::{Serialize, Deserialize};

#[derive(Serialize, Deserialize)]
struct Config {
    name: String,
    port: u16,
    enabled: bool,
}

let config = Config {
    name: "server".to_string(),
    port: 8080,
    enabled: true,
};

// Serialize to kJSON
let json = to_string_pretty(&config)?;

// Deserialize from kJSON
let parsed: Config = from_str(&json)?;
```

## Value API

The `Value` enum provides a dynamic representation of kJSON data:

```rust
use kjson::{Value, parse};
use std::collections::HashMap;

// Build values programmatically
let mut obj = HashMap::new();
obj.insert("name".to_string(), Value::String("test".to_string()));
obj.insert("count".to_string(), Value::Number(42.0));
let value = Value::Object(obj);

// Access values
match &value {
    Value::Object(map) => {
        if let Some(Value::String(name)) = map.get("name") {
            println!("Name: {}", name);
        }
    }
    _ => {}
}

// Convert to string
let json = kjson::to_string(&value)?;
```

## Error Handling

The library provides detailed error messages with position information:

```rust
match parse("{invalid json}") {
    Ok(_) => {}
    Err(kjson::Error::ParseError { position, message }) => {
        println!("Parse error at position {}: {}", position, message);
    }
    Err(e) => println!("Error: {}", e),
}
```

## Performance Considerations

- The parser is optimized for correctness over raw speed
- Extended type checking adds overhead compared to standard JSON parsers
- For performance-critical applications processing standard JSON, consider using `serde_json`
- Pretty printing sorts object keys for consistent output

## Differences from Standard JSON

1. **Extended Types**: BigInt (`n`), Decimal128 (`m`), unquoted UUIDs, Instants, and Durations
2. **JSON5 Syntax**: Comments, unquoted keys, trailing commas
3. **Relaxed Parsing**: More forgiving of common syntax patterns
4. **Type Preservation**: Extended types maintain precision and semantics

## kJSON Format Examples

```javascript
{
    // Line comment
    id: 550e8400-e29b-41d4-a716-446655440000,     // Unquoted UUID
    bigNumber: 123456789012345678901234567890n,    // BigInt with 'n' suffix
    price: 99.99m,                                 // Decimal128 with 'm' suffix
    created: 2025-01-10T12:00:00.123456789Z,       // Nanosecond-precision Instant
    timeout: PT1H30M,                              // ISO 8601 Duration
    active: true,
    tags: ["new", "sale"],
    metadata: {
        version: 1,
        /* Block comment */
    },  // Trailing comma
}
```

## Future Features

- **Derive Macros**: Custom derive for `#[kjson]` and `#[serde(rename)]` attributes
- **Streaming Parser**: For processing large kJSON files
- **Binary Format**: kjsonb for efficient storage and transmission
- **Schema Validation**: Type-safe parsing with schemas

## Contributing

This Rust client is part of the larger kJSON project. See the main repository for contribution guidelines.

## License

MIT License - see LICENSE file in the root repository.