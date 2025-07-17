# kJSON Go Client

A Go implementation of the kJSON (Kind JSON) specification with extended type support including BigInt, Decimal128, UUID, and Date types.

## Features

- **BigInt Support** - Handle large integers without precision loss (with `n` suffix)
- **Decimal128 Support** - High-precision decimal numbers (with `m` suffix)  
- **UUID Support** - Native UUID parsing and generation (v4 and v7)
- **Instant Support** - Nanosecond-precision timestamps in Zulu time (UTC)
- **Duration Support** - ISO 8601 duration format with nanosecond precision
- **JSON5 Syntax** - Unquoted keys, trailing commas, comments (parsing only)
- **Struct Tags** - Support for `kjson` tags with `json` tag fallback
- **encoding/json Compatibility** - Drop-in replacement with familiar API

## Installation

```bash
go get github.com/atikayda/kjson/clients/golang
```

## Quick Start

```go
package main

import (
    "fmt"
    "time"
    
    "github.com/atikayda/kjson/clients/golang"
    "github.com/google/uuid"
)

type User struct {
    ID       uuid.UUID    `kjson:"id"`
    BigNum   *kjson.BigInt `kjson:"bigNumber"`
    Balance  *kjson.Decimal128 `kjson:"balance"`
    Created  *kjson.Instant  `kjson:"created"`
    Name     string       `kjson:"name"`
    Active   bool         `json:"active"` // Falls back to json tag
}

func main() {
    user := User{
        ID:      kjson.UUIDv4(),
        BigNum:  kjson.NewBigInt(123456789012345678),
        Balance: mustDecimal("99.99"),
        Created: kjson.NewInstant(time.Now()),
        Name:    "Alice",
        Active:  true,
    }
    
    // Marshal to kJSON
    data, _ := kjson.Marshal(user)
    fmt.Println(string(data))
    // Output: {id:550e8400-e29b-41d4-a716-446655440000,bigNumber:123456789012345678n,balance:99.99m,created:2025-01-10T12:00:00Z,name:"Alice",active:true}
    
    // Parse back
    var parsed User
    kjson.Unmarshal(data, &parsed)
    fmt.Printf("Parsed: %+v\n", parsed)
}

func mustDecimal(s string) *kjson.Decimal128 {
    d, _ := kjson.NewDecimal128(s)
    return d
}
```

## Extended Types

### BigInt

For integers larger than what `int64` can handle:

```go
// Create BigInt
bigInt := kjson.NewBigInt(123456789012345678)

// Marshal
data, _ := kjson.Marshal(bigInt)
// Output: 123456789012345678n

// Parse
var parsed kjson.BigInt
kjson.Unmarshal([]byte("123456789012345678n"), &parsed)
```

### Decimal128

For high-precision decimal arithmetic:

```go
// Create Decimal128
decimal, _ := kjson.NewDecimal128("99.99")

// Marshal
data, _ := kjson.Marshal(decimal)
// Output: 99.99m

// Parse
var parsed kjson.Decimal128
kjson.Unmarshal([]byte("99.99m"), &parsed)
```

### UUID

Native UUID support with generation:

```go
// Generate UUIDs
uuid4 := kjson.UUIDv4() // Random UUID
uuid7 := kjson.UUIDv7() // Timestamp-based UUID

// Marshal
data, _ := kjson.Marshal(uuid4)
// Output: 550e8400-e29b-41d4-a716-446655440000

// Parse
var parsed uuid.UUID
kjson.Unmarshal(data, &parsed)
```

### Instant

Nanosecond-precision timestamps in Zulu time (UTC):

```go
// Create Instant
instant := kjson.NewInstant(time.Now())

// Marshal
data, _ := kjson.Marshal(instant)
// Output: 2025-01-10T12:00:00.123456789Z

// Parse
var parsed kjson.Instant
kjson.Unmarshal([]byte("2025-01-10T12:00:00.123456789Z"), &parsed)
```

### Duration

ISO 8601 duration format with nanosecond precision:

```go
// Create Duration
duration := kjson.NewDuration(time.Hour * 2)

// Marshal
data, _ := kjson.Marshal(duration)
// Output: PT2H

// Parse
var parsed kjson.Duration
kjson.Unmarshal([]byte("PT1H30M"), &parsed)
```

## Struct Tags

The Go client supports `kjson` struct tags with fallback to `json` tags:

```go
type Example struct {
    ID          string `kjson:"id"`           // Uses kjson tag
    Name        string `json:"name"`          // Falls back to json tag  
    Value       int    `kjson:"val"`          // Custom kjson name
    Internal    string `kjson:"-"`            // Skipped
    NoTag       string                        // Uses field name
    OmitEmpty   string `kjson:"opt,omitempty"` // Omit if empty
}
```

## API Reference

### Core Functions

```go
// Marshal Go values to kJSON bytes
func Marshal(v interface{}) ([]byte, error)

// Unmarshal kJSON bytes to Go values
func Unmarshal(data []byte, v interface{}) error

// Marshal with indentation (pretty printing)
func MarshalIndent(v interface{}, prefix, indent string) ([]byte, error)
```

### Type Constructors

```go
// BigInt
func NewBigInt(n int64) *BigInt

// Decimal128
func NewDecimal128(s string) (*Decimal128, error)
func NewDecimal128FromFloat(f float64) *Decimal128

// Instant
func NewInstant(t time.Time) *Instant

// Duration
func NewDuration(t time.Duration) *Duration

// UUID Generation
func UUIDv4() uuid.UUID  // Random UUID
func UUIDv7() uuid.UUID  // Timestamp-based UUID
```

### Utility Functions

```go
// UUID utilities
func IsValidUUID(s string) bool
func UUIDVersion(u uuid.UUID) int
func UUIDBytes(u uuid.UUID) []byte
func UUIDFromBytes(bytes []byte) (uuid.UUID, error)
```

## kJSON Format Examples

The Go client can parse and generate these kJSON formats:

```javascript
{
  // Comments are supported in parsing
  id: 550e8400-e29b-41d4-a716-446655440000,      // Unquoted UUID
  bigNumber: 123456789012345678901234567890n,     // BigInt with 'n' suffix
  price: 99.99m,                                  // Decimal128 with 'm' suffix  
  created: 2025-01-10T12:00:00.123456789Z,        // Nanosecond-precision Instant
  timeout: PT1H30M,                               // ISO 8601 Duration
  active: true,                                   // Standard types
  tags: ["new", "sale"],                          // Arrays
  metadata: {                                     // Nested objects
    version: 1,
    // Trailing commas allowed
  },
}
```

## Performance

The Go client prioritizes correctness and compatibility over raw performance. For performance-critical applications processing standard JSON without extended types, continue using the standard `encoding/json` package.

Benchmarks show approximately 2-3x slower performance compared to `encoding/json` due to the additional type checking and extended parsing logic.

## Compatibility

- **Go Version**: Requires Go 1.18+ (for generics in UUID library)
- **Dependencies**: 
  - `github.com/google/uuid` for UUID support
- **Standards**: Compatible with kJSON specification and JSON5 parsing features

## Error Handling

The library provides detailed error messages for parsing failures:

```go
data := []byte(`{invalid json}`)
var result map[string]interface{}
err := kjson.Unmarshal(data, &result)
if err != nil {
    // err is of type *kjson.ParseError with offset information
    fmt.Printf("Parse error: %v\n", err)
}
```

## Contributing

This Go client is part of the larger kJSON project. See the main repository for contribution guidelines and project information.

## License

MIT License - see LICENSE file in the root repository.