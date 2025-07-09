# kJSON - Kind JSON

<div align="center">
  <img src="assets/kJSON.webp" alt="kJSON Logo" width="160" height="160">
  <br><br>
  <strong>🎯 Developer-friendly JSON parser with BigInt, Date & JSON5 support</strong>
  <br><br>
  
  [![JSR](https://jsr.io/badges/@atikayda/kjson)](https://jsr.io/@atikayda/kjson)
  [![GitHub](https://img.shields.io/badge/GitHub-atikayda/kjson-blue?logo=github)](https://github.com/atikayda/kjson)
  [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
  [![Deno](https://img.shields.io/badge/Deno-1.45+-brightgreen?logo=deno)](https://deno.land)
  
</div>

---

**kJSON** is a powerful, developer-friendly JSON parser and stringifier with extended type support for Deno and modern JavaScript/TypeScript applications. It handles all the pain points of standard JSON while maintaining full compatibility.

## ✨ Why kJSON?

Standard JSON is limiting. kJSON is **kind** to developers by supporting:

- 🔢 **BigInt Values** - Handle large integers without precision loss
- 📅 **Date Objects** - Automatic ISO timestamp parsing and serialization  
- 💬 **Comments** - Single-line and multi-line comments for self-documenting data
- 🎯 **Flexible Syntax** - Unquoted keys, trailing commas, single quotes
- 🛡️ **Type Safety** - Full TypeScript support with comprehensive type definitions
- ⚡ **High Performance** - Optimized tokenizer with reasonable overhead
- 🔄 **Drop-in Replacement** - Works with standard JSON out of the box

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
  lastLogin: 2025-01-15T10:30:00.000Z,  // Date parsing
  settings: {
    theme: 'dark',
    notifications: true,
  },  // trailing comma OK
}`);

console.log(typeof data.balance);    // "bigint"
console.log(data.lastLogin instanceof Date); // true
```

### Blockchain/Crypto Use Case
```typescript
// Perfect for blockchain applications
const blockData = parse(`{
  // Solana block data
  slot: 250845123n,
  blockTime: 1703635200000n,
  transactions: [
    {
      signature: "5J7ZcvZ...",
      fee: 5000n,
      timestamp: 2025-01-01T00:00:00.000Z,
    }
  ],
  // Validator info
  leader: "validator1.sol"
}`);

console.log(blockData.slot);  // 250845123n (BigInt preserved)
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
  timestamp: new Date(),
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
- `parseDates: boolean` - Parse ISO date strings as Date objects (default: `true`)

**Returns:** `any` - The parsed JavaScript value with preserved types

```typescript
const result = parse(`{
  name: "Alice",
  balance: 1234567890123456789n,  // BigInt preserved
  lastLogin: 2025-01-15T10:30:00.000Z,  // Date object
  // User preferences
  settings: {
    theme: 'dark',
    notifications: true,
  },  // trailing comma allowed
}`, {
  allowTrailingCommas: true,
  allowComments: true,
  allowUnquotedKeys: true,
  parseDates: true
});

// Types are preserved
console.log(typeof result.balance);    // "bigint"
console.log(result.lastLogin instanceof Date); // true
```

#### `stringify(value: any, options?: KJsonStringifyOptions): string`

Convert a JavaScript value to a kJSON string with intelligent formatting.

**Parameters:**
- `value: any` - The JavaScript value to stringify
- `options?: KJsonStringifyOptions` - Optional stringification configuration

**Options:**
- `bigintSuffix: boolean` - Include BigInt values with 'n' suffix (default: `true`)
- `serializeDates: boolean` - Convert Date objects to ISO strings (default: `true`)
- `space: string | number` - Indentation for pretty printing (default: `0`)
- `quoteKeys: boolean` - Quote all object keys (default: `false`)

**Returns:** `string` - The kJSON string representation

```typescript
const json = stringify({
  userId: 12345n,
  timestamp: new Date(),
  metadata: { 
    version: "1.0",
    active: true 
  }
}, {
  space: 2,           // Pretty print with 2 spaces
  bigintSuffix: true, // Keep 'n' suffix for BigInt
  serializeDates: true, // Dates as ISO strings
  quoteKeys: false    // Clean unquoted keys
});

// Output:
// {
//   userId: 12345n,
//   timestamp: 2025-01-15T10:30:00.000Z,
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
| `Date` | ISO timestamp literals | `2025-01-01T00:00:00.000Z` | Automatic parsing |
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
// Mix single and double quotes freely
const data = parse(`{
  message: 'Hello "world"',
  path: "/home/user",
  template: 'This is ${variable} interpolation'
}`);
```

## 🎯 Real-World Use Cases

### 🗄️ Large-Scale Data Processing
```typescript
// Handle large integers common in data processing
const dataMetrics = parse(`{
  totalRecords: 1234567890123456789n,
  processedAt: 2025-01-15T10:30:00.000Z,
  batchSize: 1000000n,
  results: {
    successful: 999999n,
    failed: 1n,
    avgProcessingTime: 0.045
  }
}`);

console.log(dataMetrics.totalRecords); // BigInt preserved
console.log(dataMetrics.processedAt);  // Date object
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
  timestamp: new Date(),
  sessionId: "sess_" + Date.now(),
  data: {
    recordCount: 999999999999999999n,
    lastUpdated: new Date(),
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
  timestamp: new Date(),
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
  startTime: 2025-01-15T10:00:00.000Z,
  powerUps: [
    {
      id: 1n,
      duration: 30000n,  // 30 seconds in milliseconds
      activatedAt: 2025-01-15T10:05:00.000Z
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
  createdAt: Date;
}

// Type-safe parsing
const config: UserConfig = parse(jsonString);

// IntelliSense works perfectly
console.log(config.timeout); // TypeScript knows this is bigint
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

## 🚀 Repository

**GitHub**: [https://github.com/atikayda/kjson](https://github.com/atikayda/kjson)  
**JSR**: [https://jsr.io/@atikayda/kjson](https://jsr.io/@atikayda/kjson)

## 📋 Changelog

### v1.0.1 (Documentation Update)
- 📖 Enhanced README with comprehensive documentation
- 🎨 Improved logo display (WebP format)
- 🌟 Added detailed use cases and examples
- 🤝 Enhanced contribution guidelines
- 🔗 Added GitHub repository links
- 📚 Better API documentation with parameters and returns

### v1.0.0 (Initial Release)
- ✨ Beautiful gradient logo design
- 🚀 Full JSON5 syntax support (comments, trailing commas, unquoted keys)
- 🔢 BigInt type support with `n` suffix
- 📅 Date object parsing and serialization
- 🛡️ Comprehensive test suite (30 tests, 100% pass rate)
- 📚 TypeScript-first design with full type definitions
- ⚡ Performance optimizations with reasonable overhead
- 📖 Complete documentation with real-world examples
- 🎯 Developer-friendly API with utility functions
- 🌐 Cross-platform support (Deno, Node.js via JSR)

---

<div align="center">
  <strong>Made with 💜 by the kJSON team</strong>
  <br>
  <em>Kind JSON for kind developers</em>
</div>