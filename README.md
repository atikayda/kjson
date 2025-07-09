# kJSON - Kind JSON

<div align="center">
  <img src="assets/kJSON.svg" alt="kJSON Logo" width="120" height="120">
</div>

A powerful, developer-friendly JSON parser and stringifier with extended type support for Deno and modern JavaScript/TypeScript applications.

**Package Name**: `@atikayda/kjson`

## Features

- **JSON5 Syntax Support**: Unquoted keys, trailing commas, and flexible syntax
- **JSONC Comments**: Single-line (`//`) and multi-line (`/* */`) comments
- **BigInt Support**: Native BigInt parsing with `n` suffix (e.g., `12345n`)
- **Date Objects**: Automatic ISO timestamp parsing and serialization
- **TypeScript-First**: Comprehensive type safety and IntelliSense support
- **Standard JSON Compatible**: Works with regular JSON out of the box
- **High Performance**: Optimized tokenizer and parser for speed
- **Flexible Options**: Configurable parsing and stringification behavior

## Installation

```bash
# Import from JSR (recommended)
import { parse, stringify, kJSON } from "jsr:@atikayda/kjson";

# Or import from Deno.land (once published)
import { parse, stringify } from "https://deno.land/x/kjson/mod.ts";

# Or import directly from GitHub
import { parse, stringify, kJSON } from "https://raw.githubusercontent.com/atikayda/kjson/main/mod.ts";
```

## Quick Start

```typescript
import { parse, stringify } from "./mod.ts";

// Parse enhanced JSON with BigInt and Date support
const data = parse(`{
  // Configuration object
  name: "MT Trading Bot",
  currentSlot: 250845123n,
  createdAt: 2025-06-12T20:22:35.328Z,
  config: {
    maxRetries: 5,
    timeout: 30000n,
  },
}`);

console.log(typeof data.currentSlot); // "bigint"
console.log(data.createdAt instanceof Date); // true

// Stringify back to enhanced JSON
const json = stringify(data, { space: 2 });
console.log(json);
```

## API Reference

### Core Functions

#### `parse(text: string, options?: KJsonParseOptions): any`

Parse a kJSON string into a JavaScript value.

**Options:**
- `allowTrailingCommas: boolean` - Allow trailing commas in objects and arrays (default: `true`)
- `allowComments: boolean` - Allow comments in the JSON (default: `true`)
- `allowUnquotedKeys: boolean` - Allow unquoted object keys (default: `true`)
- `parseDates: boolean` - Parse ISO date strings as Date objects (default: `true`)

```typescript
const result = parse(`{
  name: "test",
  value: 42n,
  timestamp: 2025-01-01T00:00:00.000Z,
  // This is a comment
  items: [1, 2, 3,]
}`, {
  allowTrailingCommas: true,
  allowComments: true,
  allowUnquotedKeys: true,
  parseDates: true
});
```

#### `stringify(value: any, options?: KJsonStringifyOptions): string`

Convert a JavaScript value to a kJSON string.

**Options:**
- `bigintSuffix: boolean` - Include BigInt values with 'n' suffix (default: `true`)
- `serializeDates: boolean` - Convert Date objects to ISO strings (default: `true`)
- `space: string | number` - Indentation for pretty printing (default: `0`)
- `quoteKeys: boolean` - Quote all object keys (default: `false`)

```typescript
const json = stringify({
  id: 12345n,
  timestamp: new Date(),
  config: { enabled: true }
}, {
  space: 2,
  bigintSuffix: true,
  serializeDates: true,
  quoteKeys: false
});
```

### kJSON Namespace

The `kJSON` namespace provides additional utility functions:

#### `kJSON.isValid(text: string, options?: KJsonParseOptions): boolean`

Check if a string is valid kJSON.

```typescript
console.log(kJSON.isValid('{ name: "test" }')); // true
console.log(kJSON.isValid('{ invalid json }')); // false
```

#### `kJSON.safeParseWith<T>(text: string, defaultValue: T, options?: KJsonParseOptions): T`

Parse with error handling that returns a default value on failure.

```typescript
const config = kJSON.safeParseWith('{ invalid }', { default: true });
console.log(config); // { default: true }
```

#### `kJSON.createParser(options: KJsonParseOptions)`

Create a custom parser with preset options.

```typescript
const strictParser = kJSON.createParser({
  allowTrailingCommas: false,
  allowComments: false,
  allowUnquotedKeys: false
});

const result = strictParser('{"name": "test"}');
```

#### `kJSON.createStringifier(options: KJsonStringifyOptions)`

Create a custom stringifier with preset options.

```typescript
const prettyStringifier = kJSON.createStringifier({
  space: 2,
  quoteKeys: true
});

const json = prettyStringifier({ name: "test" });
```

## Supported Types

### Standard JSON Types
- `string` - Text values
- `number` - Numeric values (including scientific notation)
- `boolean` - `true` and `false`
- `null` - Null values
- `object` - Objects and nested structures
- `array` - Arrays with mixed types

### Extended Types
- `bigint` - Large integers with `n` suffix: `123456789012345678901234567890n`
- `Date` - ISO timestamp literals: `2025-01-01T00:00:00.000Z`
- `undefined` - Undefined values (parsing only)

## JSON5 Features

### Unquoted Keys
```typescript
const data = parse(`{
  name: "value",
  camelCase: "works",
  snake_case: "also works"
}`);
```

### Trailing Commas
```typescript
const data = parse(`{
  a: 1,
  b: 2,
  c: [1, 2, 3,],
}`);
```

### Comments
```typescript
const data = parse(`{
  // Single-line comment
  name: "test",
  /* Multi-line
     comment */
  value: 42
}`);
```

### Flexible Quotes
```typescript
const data = parse(`{
  single: 'quotes work',
  double: "quotes also work",
  mixed: 'both "types" together'
}`);
```

## Use Cases

### Blockchain Applications
```typescript
// Perfect for blockchain data with BigInt support
const blockData = parse(`{
  slot: 250845123n,
  blockTime: 1703635200000n,
  transactions: [
    {
      signature: "abc123...",
      fee: 5000n,
      timestamp: 2025-01-01T00:00:00.000Z
    }
  ]
}`);
```

### Configuration Files
```typescript
// Config files with comments and flexible syntax
const config = parse(`{
  // Database configuration
  database: {
    host: "localhost",
    port: 5432,
    maxConnections: 100n,
  },
  
  // API settings
  api: {
    rateLimit: 1000n,
    timeout: 30000n,
  },
}`);
```

### API Responses
```typescript
// Type-preserving API responses
const response = stringify({
  userId: 12345678901234567890n,
  timestamp: new Date(),
  data: { /* ... */ }
});
```

### WebSocket Messages
```typescript
// Real-time data with mixed types
const message = stringify({
  type: "price_update",
  tokenId: 987654321n,
  price: 0.00123456,
  timestamp: new Date(),
  volume24h: 1234567890123456789n
});
```

## Performance

kJSON is optimized for performance while maintaining feature richness:

- **Tokenizer**: Efficient single-pass lexical analysis
- **Parser**: Recursive descent parser with minimal overhead
- **Memory**: Lazy evaluation and efficient string handling
- **Speed**: Comparable to JSON.parse for standard JSON, with reasonable overhead for extended features

## Error Handling

kJSON provides detailed error messages with position information:

```typescript
try {
  parse('{ invalid: json }');
} catch (error) {
  console.log(error.message); // "Expected ',' or '}' at position 15"
}
```

## TypeScript Support

Full TypeScript support with comprehensive type definitions:

```typescript
interface Config {
  name: string;
  maxRetries: number;
  timeout: bigint;
  createdAt: Date;
}

const config: Config = parse(jsonString);
```

## Contributing

1. Fork the repository
2. Create a feature branch
3. Add tests for new features
4. Run `deno task test` to ensure all tests pass
5. Submit a pull request

## License

MIT License - see LICENSE file for details.

## Brand Assets

The kJSON logo and brand assets are available in the [`assets/`](assets/) directory:
- **SVG**: Scalable vector format (recommended)
- **PNG**: High-quality raster format
- **WebP**: Modern web-optimized format

## Changelog

### v1.0.0
- Initial release with beautiful gradient logo design
- Full JSON5 syntax support
- BigInt and Date type support
- Comprehensive test suite (30 tests)
- TypeScript-first design
- Performance optimizations
- Complete documentation and examples