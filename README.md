# kJSON - Kind JSON

<div align="center">
  <img src="assets/kJSON.webp" alt="kJSON Logo" width="160" height="160">
  <br><br>
  <strong>🎯 Developer-friendly JSON parser with BigInt, Decimal128, Instant, Duration, UUID & JSON5 support</strong>
  <br><br>
  
  [![JSR](https://jsr.io/badges/@atikayda/kjson)](https://jsr.io/@atikayda/kjson)
  [![GitHub](https://img.shields.io/badge/GitHub-atikayda/kjson-blue?logo=github)](https://github.com/atikayda/kjson)
  [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
  [![Deno](https://img.shields.io/badge/Deno-1.45+-brightgreen?logo=deno)](https://deno.land)
  
</div>

---

**kJSON** is a powerful, developer-friendly JSON parser and stringifier with extended type support for Deno and modern JavaScript/TypeScript applications. It handles all the pain points of standard JSON while maintaining full compatibility.

## 🌍 Multi-Language Support

kJSON is available across multiple programming languages with consistent APIs:

| Language | Status | Features | Documentation |
|----------|--------|----------|---------------|
| **TypeScript/Deno** | ✅ Production | Full feature set with Temporal API support | [Main README](README.md) |
| **C** | ✅ Production | Full feature set with nanosecond precision | [C Client README](clients/c/README.md) |
| **Python** | ✅ Production | Full feature set with datetime integration | [Python Client README](clients/python/README.md) |
| **Rust** | ✅ Production | Full feature set with chrono integration | [Rust Client README](clients/rust/README.md) |
| **Golang** | ✅ Production | Full feature set with time.Time integration | [Golang Client README](clients/golang/README.md) |
| **PostgreSQL** | ✅ Production | Native types + unified temporal system | [PostgreSQL Client README](clients/postgres/README.md) |

All clients support the same core features: **BigInt**, **Decimal128**, **UUID**, **Instant**, **Duration**, and **JSON5 syntax**.

## ✨ Why kJSON?

Standard JSON is limiting. kJSON is **kind** to developers by supporting:

- 🔢 **BigInt Values** - Handle large integers without precision loss
- 💰 **Decimal128** - IEEE 754 decimal arithmetic for exact calculations (no 0.1 + 0.2 errors!)
- ⏰ **Nanosecond Instants** - Precise timestamps in Zulu time with nanosecond precision
- ⏳ **ISO 8601 Durations** - Time spans with nanosecond precision (PT1H30M, P1DT2H3M4S)
- 🆔 **UUID Support** - Parse and stringify UUIDs without quotes and serialization  
- 💬 **Comments** - Single-line and multi-line comments for self-documenting data
- 🎯 **Flexible Syntax** - Unquoted keys, trailing commas, single quotes
- 🛡️ **Type Safety** - Full TypeScript support with comprehensive type definitions
- ⚡ **High Performance** - Optimized tokenizer with reasonable overhead
- 🔄 **Drop-in Replacement** - Works with standard JSON out of the box
- 🌐 **Cross-Platform** - Consistent API across TypeScript, C, Python, Rust, Golang, and PostgreSQL

## 🚀 Installation

```typescript
// Import from JSR (recommended)
import { parse, stringify, kJSON } from "jsr:@atikayda/kjson";

// Or import from GitHub
import { parse, stringify, kJSON } from "https://raw.githubusercontent.com/atikayda/kjson/main/mod.ts";
```

## 🎯 Quick Examples

### Basic Usage
```typescript
import { parse, stringify } from "jsr:@atikayda/kjson";

// Parse with all features enabled
const data = parse(`{
  // User configuration
  name: "Alice",
  balance: 1234567890123456789n,  // BigInt support
  creditLimit: 5000.00m,           // Decimal128 for exact money
  userId: 550e8400-e29b-41d4-a716-446655440000,  // UUID
  lastLogin: 2025-01-15T10:30:00.123456789Z,  // Nanosecond instant
  sessionTimeout: PT1H30M,         // ISO 8601 duration
  settings: {
    theme: 'dark',
    notifications: true,
  },  // trailing comma OK
}`);

console.log(typeof data.balance);      // "bigint"
console.log(data.creditLimit);        // Decimal128 { value: "5000.00" }
console.log(data.lastLogin.toString()); // "2025-01-15T10:30:00.123456789Z"
console.log(data.sessionTimeout.toString()); // "PT1H30M"
```

### Blockchain/Crypto Use Case
```typescript
// Perfect for blockchain applications
const blockData = parse(`{
  // Solana block data
  slot: 250845123n,
  blockTime: 1703635200000n,
  timestamp: 2025-01-01T00:00:00.123456789Z,  // Nanosecond precision
  processingTime: PT0.000000123S,             // Duration in nanoseconds
  transactions: [
    {
      signature: "5J7ZcvZ...",
      fee: 5000n,
      executionTime: PT0.000045S,  // 45 microseconds
      timestamp: 2025-01-01T00:00:00.123456789Z,
    }
  ],
  // Validator info
  leader: "validator1.sol"
}`);

console.log(blockData.slot);  // 250845123n (BigInt preserved)
console.log(blockData.timestamp.epochNanoseconds); // nanosecond precision
```

### Financial/Trading Use Case
```typescript
import { Decimal128 } from "@atikayda/kjson/decimal128";

// Perfect for financial calculations
const invoice = parse(`{
  id: "INV-001",
  items: [
    { name: "Widget", price: 19.99m, quantity: 3 },
    { name: "Gadget", price: 49.95m, quantity: 2 }
  ],
  subtotal: 159.87m,
  tax: 12.79m,
  total: 172.66m
}`);

// Exact decimal arithmetic - no floating point errors!
const item1Total = invoice.items[0].price.multiply(3);
console.log(item1Total.toString()); // "59.97" (exactly!)

// Demonstrate the problem with regular JavaScript numbers
console.log(0.1 + 0.2);  // 0.30000000000000004 (oops!)
const d1 = new Decimal128("0.1");
const d2 = new Decimal128("0.2");
console.log(d1.add(d2).toString());  // "0.3" (exact!)
```

### Configuration Files
```typescript
// Configuration with comments and flexible syntax
const config = parse(`{
  // Database settings
  database: {
    host: "localhost",
    port: 5432,
    maxConnections: 100n,
    timeout: 30000n,
  },
  
  // API configuration
  api: {
    rateLimit: 1000n,
    timeout: 5000n,
    retries: 3,
  },
  
  // Feature flags
  features: {
    newUI: true,
    // analytics: false,  // commented out
    beta: true,
  }
}`);
```

### Real-time Data
```typescript
// WebSocket message with mixed types
const message = stringify({
  type: "trade_update",
  tokenId: 987654321n,
  price: 0.00123456,
  timestamp: Instant.now(),
  volume24h: 1234567890123456789n,
  metadata: {
    exchange: "DEX",
    pair: "TOKEN/USDC"
  }
}, { space: 2 });

// Sends as kJSON with preserved types
socket.send(message);
```

## 📚 API Reference

### Core Functions

#### `parse(text: string, options?: KJsonParseOptions): any`

Parse a kJSON string into a JavaScript value with full type preservation.

**Parameters:**
- `text: string` - The kJSON string to parse
- `options?: KJsonParseOptions` - Optional parsing configuration

**Options:**
- `allowTrailingCommas: boolean` - Allow trailing commas in objects and arrays (default: `true`)
- `allowComments: boolean` - Allow `//` and `/* */` comments (default: `true`)
- `allowUnquotedKeys: boolean` - Allow unquoted object keys (default: `true`)
- `parseInstants: boolean` - Parse ISO timestamp strings as Instant objects (default: `true`)
- `parseDurations: boolean` - Parse ISO 8601 duration strings as Duration objects (default: `true`)

**Returns:** `any` - The parsed JavaScript value with preserved types

```typescript
const result = parse(`{
  name: "Alice",
  balance: 1234567890123456789n,  // BigInt preserved
  lastLogin: 2025-01-15T10:30:00.123456789Z,  // Instant object
  // User preferences
  settings: {
    theme: 'dark',
    notifications: true,
  },  // trailing comma allowed
}`, {
  allowTrailingCommas: true,
  allowComments: true,
  allowUnquotedKeys: true,
  parseInstants: true
});

// Types are preserved
console.log(typeof result.balance);    // "bigint"
console.log(result.lastLogin.toString()); // "2025-01-15T10:30:00.123456789Z"
```

#### `stringify(value: any, options?: KJsonStringifyOptions): string`

Convert a JavaScript value to a kJSON string with intelligent formatting.

**Parameters:**
- `value: any` - The JavaScript value to stringify
- `options?: KJsonStringifyOptions` - Optional stringification configuration

**Options:**
- `bigintSuffix: boolean` - Include BigInt values with 'n' suffix (default: `true`)
- `serializeInstants: boolean` - Convert Instant objects to ISO strings (default: `true`)
- `serializeDurations: boolean` - Convert Duration objects to ISO 8601 strings (default: `true`)
- `space: string | number` - Indentation for pretty printing (default: `0`)
- `quoteKeys: boolean` - Quote all object keys (default: `false`)

**Returns:** `string` - The kJSON string representation

```typescript
const json = stringify({
  userId: 12345n,
  timestamp: Instant.now(),
  timeout: Duration.from("PT30M"),
  metadata: { 
    version: "1.0",
    active: true 
  }
}, {
  space: 2,             // Pretty print with 2 spaces
  bigintSuffix: true,   // Keep 'n' suffix for BigInt
  serializeInstants: true, // Instants as ISO strings
  serializeDurations: true, // Durations as ISO 8601 strings
  quoteKeys: false      // Clean unquoted keys
});

// Output:
// {
//   userId: 12345n,
//   timestamp: 2025-01-15T10:30:00.123456789Z,
//   timeout: PT30M,
//   metadata: {
//     version: "1.0",
//     active: true
//   }
// }
```

### kJSON Namespace Utilities

The `kJSON` namespace provides additional utility functions for advanced use cases:

#### `kJSON.isValid(text: string, options?: KJsonParseOptions): boolean`

Validate if a string is valid kJSON without throwing errors.

```typescript
// Quick validation
console.log(kJSON.isValid('{ name: "Alice", age: 25 }')); // true
console.log(kJSON.isValid('{ invalid json }')); // false

// With options
console.log(kJSON.isValid('{ name: "test", }', { 
  allowTrailingCommas: false 
})); // false
```

#### `kJSON.safeParseWith<T>(text: string, defaultValue: T, options?: KJsonParseOptions): T`

Parse with graceful error handling - returns default value on failure.

```typescript
// Safe parsing with fallback
const userConfig = kJSON.safeParseWith(
  userInput, 
  { theme: 'light', notifications: true }
);

// Always returns valid config object
console.log(userConfig.theme); // Works even if userInput is invalid
```

#### `kJSON.createParser(options: KJsonParseOptions)`

Create a reusable parser with preset options.

```typescript
// Strict parser for production data
const strictParser = kJSON.createParser({
  allowTrailingCommas: false,
  allowComments: false,
  allowUnquotedKeys: false
});

// Flexible parser for config files
const configParser = kJSON.createParser({
  allowTrailingCommas: true,
  allowComments: true,
  allowUnquotedKeys: true
});

const prodData = strictParser('{"name": "Alice", "id": 123}');
const config = configParser('{ name: "App", debug: true, }');
```

#### `kJSON.createStringifier(options: KJsonStringifyOptions)`

Create a reusable stringifier with preset formatting.

```typescript
// Pretty printer for logs
const prettyStringifier = kJSON.createStringifier({
  space: 2,
  quoteKeys: true
});

// Compact stringifier for network transmission
const compactStringifier = kJSON.createStringifier({
  space: 0,
  quoteKeys: false
});

const data = { id: 123n, timestamp: new Date() };
console.log(prettyStringifier(data));  // Formatted
console.log(compactStringifier(data)); // Compact
```

## 🔧 Supported Types & Features

### Standard JSON Types
| Type | Description | Example |
|------|-------------|---------|
| `string` | Text values | `"Hello world"` |
| `number` | Numeric values (including scientific notation) | `42`, `3.14`, `1.23e-4` |
| `boolean` | Boolean values | `true`, `false` |
| `null` | Null value | `null` |
| `object` | Objects and nested structures | `{ key: "value" }` |
| `array` | Arrays with mixed types | `[1, "two", 3.0]` |

### 🚀 Extended Types (kJSON Exclusive)
| Type | Description | Example | Benefit |
|------|-------------|---------|---------|
| `bigint` | Large integers with `n` suffix | `123456789012345678901234567890n` | No precision loss |
| `Decimal128` | IEEE 754 decimals with `m` suffix | `99.99m`, `0.1m` | Exact decimal arithmetic |
| `Instant` | Nanosecond-precision timestamps in Zulu time | `2025-01-01T00:00:00.123456789Z` | Precise timestamps |
| `Duration` | ISO 8601 duration strings with nanosecond precision | `PT1H30M`, `PT0.000000123S` | Time span representation |
| `UUID` | Unquoted UUID literals | `550e8400-e29b-41d4-a716-446655440000` | Native UUID support |
| `undefined` | Undefined values | `undefined` | JavaScript compatibility |

### 🎨 JSON5 Syntax Features

#### Unquoted Keys
```typescript
// Clean, readable syntax
const data = parse(`{
  name: "Alice",
  userId: 12345,
  isActive: true
}`);
```

#### Trailing Commas
```typescript
// No more syntax errors from trailing commas
const data = parse(`{
  database: "prod",
  timeout: 5000,
  retries: 3,        // <- trailing comma OK
}`);
```

#### Comments Support
```typescript
// Self-documenting configuration
const config = parse(`{
  // Database configuration
  database: {
    host: "localhost",
    port: 5432,
    /* Connection pooling settings
       for production use */
    maxConnections: 100,
  },
  
  // Feature flags
  features: {
    newUI: true,
    // beta: false,  // Disabled for now
  }
}`);
```

#### Flexible Quotes
```typescript
// Mix single, double, and backtick quotes freely
const data = parse(`{
  message: 'Hello "world"',
  path: "/home/user",
  multiline: \`This is line 1
This is line 2\`,
  mixed: \`He said "Hello" and she replied 'Hi'\`
}`);

// Smart quote selection during stringify - chooses the quote type
// that requires the least escaping. In case of ties:
// single quotes > double quotes > backticks
stringify({text: "simple"});           // {text: 'simple'}
stringify({text: "it's nice"});        // {text: "it's nice"}
stringify({text: 'He said "hi"'});     // {text: 'He said "hi"'}
stringify({text: `Mix 'both' "types"`}); // {text: \`Mix 'both' "types"\`}
```

### ⏰ Instant & Duration Support

kJSON provides comprehensive support for nanosecond-precision timestamps (Instant) and time spans (Duration) with compatibility for both Temporal API and custom polyfills.

#### Requirements
- Works with any Deno version
- Uses Temporal API when available, falls back to compatible polyfills
- Supports nanosecond precision in all environments

#### Basic Usage
```typescript
import { parse, stringify, Instant, Duration } from "jsr:@atikayda/kjson";

// Parse timestamps and durations
const data = parse(`{
  created: 2025-01-01T10:00:00.000Z,
  updated: 2025-01-01T12:30:45.123456789Z,  // Nanosecond precision
  timezone_aware: 2025-01-01T15:30:00+05:30, // Converted to Zulu time
  sessionTimeout: PT1H30M,                  // 1 hour 30 minutes
  processingTime: PT0.000000123S            // 123 nanoseconds
}`, {
  parseInstants: true,
  parseDurations: true
});

console.log(data.created.toString()); // "2025-01-01T10:00:00Z"
console.log(data.updated.toString()); // "2025-01-01T12:30:45.123456789Z"
console.log(data.sessionTimeout.toString()); // "PT1H30M"
```

#### Creating Instants and Durations
```typescript
// Create from current time
const now = Instant.now();
console.log(now.toString()); // "2025-01-15T10:30:00.123456789Z"

// Create from epoch values
const instant = Instant.fromEpochNanoseconds(1642687200000000000n);
const millis = Instant.fromEpochMilliseconds(1642687200000);

// Create durations
const duration = Duration.from("PT1H30M");        // 1 hour 30 minutes
const nanoseconds = Duration.from("PT0.000000123S"); // 123 nanoseconds
const custom = Duration.from({ hours: 2, minutes: 30 });
```

#### Serialization
```typescript
const instant = Instant.now();
const duration = Duration.from("PT2H30M");

const json = stringify({
  timestamp: instant,
  timeout: duration,
  event: "system_start"
}, {
  serializeInstants: true,
  serializeDurations: true
});

// Output: { timestamp: 2025-01-15T10:30:00.123456789Z, timeout: PT2H30M, event: "system_start" }
```

#### Precision & Formats
```typescript
// Supports various ISO 8601 formats
const instantFormats = [
  "2025-01-01T00:00:00.000Z",           // Milliseconds
  "2025-01-01T00:00:00.123456789Z",    // Nanoseconds
  "2025-01-01T00:00:00+00:00",         // Timezone offset
  "2025-01-01T12:30:45.123+05:30"     // Regional timezone (converted to Zulu)
];

const durationFormats = [
  "PT1H30M",                            // 1 hour 30 minutes
  "P1DT2H3M4S",                        // 1 day 2 hours 3 minutes 4 seconds
  "PT0.000000123S",                    // 123 nanoseconds
  "PT1.5H"                             // 1.5 hours
];

instantFormats.forEach(format => {
  const parsed = parse(`{ time: ${format} }`);
  console.log(parsed.time.toString()); // Always in Zulu time
});
```

#### Mathematical Operations
```typescript
// Instant arithmetic
const start = Instant.now();
const duration = Duration.from("PT30M");
const end = start.add(duration);

// Duration arithmetic  
const d1 = Duration.from("PT1H");
const d2 = Duration.from("PT30M");
const total = d1.add(d2); // PT1H30M

// Time differences
const diff = end.since(start); // Returns Duration
console.log(diff.toString()); // "PT30M"
```

#### Performance & Compatibility
- **Temporal API**: Uses native Temporal.Instant/Duration when available
- **Polyfill**: Compatible polyfills when Temporal API unavailable
- **Nanosecond precision**: Full support for high-precision timestamps
- **Zero overhead**: When not used, no performance impact
- **Cross-platform**: Works in all Deno environments

```typescript
// Works in all environments
const data = parse(`{ created: 2025-01-01T00:00:00.123456789Z }`);
console.log(data.created.epochNanoseconds); // Works with both Temporal and polyfill
```

## 🎯 Real-World Use Cases

### 🗄️ Large-Scale Data Processing
```typescript
// Handle large integers common in data processing
const dataMetrics = parse(`{
  totalRecords: 1234567890123456789n,
  processedAt: 2025-01-15T10:30:00.123456789Z,
  batchSize: 1000000n,
  results: {
    successful: 999999n,
    failed: 1n,
    avgProcessingTime: 0.045
  }
}`);

console.log(dataMetrics.totalRecords); // BigInt preserved
console.log(dataMetrics.processedAt.toString());  // "2025-01-15T10:30:00.123456789Z"
```

### ⚙️ Configuration Management
```typescript
// Application config with comments and flexible syntax
const appConfig = parse(`{
  // Database settings
  database: {
    host: "localhost",
    port: 5432,
    maxConnections: 100n,
    timeout: 30000n,
  },
  
  // Cache configuration
  cache: {
    host: "redis.example.com",
    port: 6379,
    ttl: 3600,  // 1 hour
  },
  
  // Feature flags
  features: {
    newDashboard: true,
    // betaFeature: false,  // Disabled for now
    analyticsV2: true,
  }
}`);
```

### 🌐 API & WebSocket Communication
```typescript
// Type-preserving API responses
const apiResponse = stringify({
  userId: 12345678901234567890n,  // Large user ID
  timestamp: Instant.now(),
  sessionId: "sess_" + Date.now(),
  data: {
    recordCount: 999999999999999999n,
    lastUpdated: Instant.now(),
    preferences: {
      theme: "dark",
      notifications: true
    }
  }
});

// WebSocket real-time updates  
const wsMessage = stringify({
  type: "data_update",
  id: 987654321n,
  value: 0.00123456,
  count: 1234567890123456789n,
  timestamp: Instant.now(),
  change: +5.67
});
```

### 🎮 Gaming & Analytics
```typescript
// Game state with large score values
const gameState = parse(`{
  playerId: 123456789012345678901234567890n,
  score: 999999999999999999n,
  level: 42,
  startTime: 2025-01-15T10:00:00.123456789Z,
  powerUps: [
    {
      id: 1n,
      duration: PT30S,  // 30 seconds
      activatedAt: 2025-01-15T10:05:00.123456789Z
    }
  ]
}`);
```

## ⚡ Performance

kJSON is optimized for performance while maintaining feature richness:

- **🚀 Tokenizer**: Efficient single-pass lexical analysis
- **🔄 Parser**: Recursive descent parser with minimal overhead  
- **💾 Memory**: Lazy evaluation and efficient string handling
- **⚡ Speed**: Comparable to JSON.parse for standard JSON, with ~2-3x overhead for extended features

```typescript
// Performance comparison (1000 iterations)
// Standard JSON: 94ms
// kJSON: 287ms (~3x overhead)
// Trade-off: Extended features + type preservation
```

## 🛡️ Error Handling

kJSON provides detailed error messages with position information for debugging:

```typescript
try {
  parse('{ invalid: json }');
} catch (error) {
  console.log(error.message); 
  // "Expected ',' or '}' at position 15"
}

// Safe parsing with fallback
const safeData = kJSON.safeParseWith(
  '{ potentially: invalid }',
  { default: 'fallback' }
);
```

## 🔷 TypeScript Support

Full TypeScript support with comprehensive type definitions:

```typescript
interface UserConfig {
  name: string;
  maxRetries: number;
  timeout: bigint;
  createdAt: Instant;
}

// Type-safe parsing
const config: UserConfig = parse(jsonString);

// IntelliSense works perfectly
console.log(config.timeout); // TypeScript knows this is bigint
console.log(config.createdAt.toString()); // TypeScript knows this is Instant
```

## 🤝 Contributing

We welcome contributions! Here's how to get started:

1. **Fork** the repository on [GitHub](https://github.com/atikayda/kjson)
2. **Clone** your fork locally
3. **Create** a feature branch: `git checkout -b feature/amazing-feature`
4. **Add tests** for new features in `tests/kjson_test.ts`
5. **Run tests**: `deno task test` (ensure all 30 tests pass)
6. **Check types**: `deno task check`
7. **Lint code**: `deno task lint`
8. **Commit changes**: `git commit -m "Add amazing feature"`
9. **Push branch**: `git push origin feature/amazing-feature`
10. **Submit** a pull request

### Development Setup
```bash
# Clone the repository
git clone https://github.com/atikayda/kjson.git
cd kjson

# Run tests
deno task test

# Run example
deno task example

# Type check
deno task check
```

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

## 🎨 Brand Assets

The kJSON logo and brand assets are available in the [`assets/`](assets/) directory:
- **SVG**: Scalable vector format (recommended for web)
- **PNG**: High-quality raster format (for general use)
- **WebP**: Modern web-optimized format (smaller file sizes)

## 🌍 Client Libraries

kJSON is available in multiple programming languages, each with full feature support:

### TypeScript/Deno (Main Library)
```typescript
import { parse, stringify, Instant, Duration } from "jsr:@atikayda/kjson";
```
- **Repository**: This repository
- **Features**: Full feature set with Temporal API compatibility
- **Documentation**: [Main README](README.md)

### C Client
```c
#include "kjson.h"
kjson_value* parsed = kjson_parse(json_string);
```
- **Path**: [`clients/c/`](clients/c/)
- **Features**: Full feature set with nanosecond precision
- **Documentation**: [C Client README](clients/c/README.md)

### Python Client
```python
import kjson
data = kjson.parse('{"timestamp": 2025-01-01T00:00:00.123456789Z}')
```
- **Path**: [`clients/python/`](clients/python/)
- **Features**: Full feature set with datetime integration
- **Documentation**: [Python Client README](clients/python/README.md)

### Rust Client
```rust
use kjson::{parse, stringify, Instant, Duration};
let data = parse(r#"{"timeout": "PT1H30M"}"#)?;
```
- **Path**: [`clients/rust/`](clients/rust/)
- **Features**: Full feature set with chrono integration
- **Documentation**: [Rust Client README](clients/rust/README.md)

### Golang Client
```go
import "github.com/atikayda/kjson/golang"
data, err := kjson.Parse(`{"balance": 123456789n}`)
```
- **Path**: [`clients/golang/`](clients/golang/)
- **Features**: Full feature set with time.Time integration
- **Documentation**: [Golang Client README](clients/golang/README.md)

### PostgreSQL Extension
```sql
CREATE EXTENSION kjson;
SELECT '{"created": 2025-01-01T00:00:00.123456789Z}'::kjson;
```
- **Path**: [`clients/postgres/`](clients/postgres/)
- **Features**: Native PostgreSQL types + unified temporal system
- **Documentation**: [PostgreSQL Client README](clients/postgres/README.md)

All clients maintain API consistency and support the same core types: **BigInt**, **Decimal128**, **UUID**, **Instant**, **Duration**, and **JSON5 syntax**.

## 🚀 Repository

**GitHub**: [https://github.com/atikayda/kjson](https://github.com/atikayda/kjson)  
**JSR**: [https://jsr.io/@atikayda/kjson](https://jsr.io/@atikayda/kjson)


---

<div align="center">
  <strong>Made with 💜 by the kJSON team</strong>
  <br>
  <em>Kind JSON for kind developers</em>
</div>