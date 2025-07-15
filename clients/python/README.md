# kJSON Python Client

Python implementation of the kJSON (Kind JSON) format with extended type support for BigInt, Decimal128, UUID, and Date types, plus JSON5 syntax features.

## Features

- 🔢 **BigInt Support** - Arbitrary precision integers with `n` suffix
- 💰 **Decimal128** - High-precision decimals with `m` suffix  
- 🆔 **UUID** - Native UUID type support (unquoted in JSON)
- 📅 **Date** - ISO 8601 date/time with timezone support
- 💬 **JSON5 Syntax** - Comments, unquoted keys, trailing commas, single quotes
- 🐍 **Pythonic API** - Drop-in replacement for Python's `json` module
- 🔄 **Round-trip Safe** - Preserves extended types through serialization

## Installation

```bash
# Install from the monorepo
cd clients/python
pip install -e .
```

## Quick Start

```python
from kjson import loads, dumps, BigInt, Decimal128, Date, uuid_v4

# Parse kJSON with extended types
data = loads('''{
    id: 550e8400-e29b-41d4-a716-446655440000,
    amount: 99999999999999999999999999n,
    price: 19.99m,
    created: 2025-01-10T12:00:00Z,
    // Comments are supported!
    active: true,
}''')

print(data['amount'])  # BigInt(99999999999999999999999999)
print(data['price'])   # Decimal128('19.99')

# Serialize with extended types
result = {
    "id": uuid_v4(),
    "balance": BigInt("123456789012345678901234567890"),
    "rate": Decimal128("0.05"),
    "timestamp": Date("2025-01-10T12:00:00Z")
}

print(dumps(result, indent=2))
```

## Extended Types

### BigInt

Arbitrary precision integers for values beyond JavaScript's safe integer range:

```python
from kjson import BigInt, dumps, loads

# Create BigInt
big = BigInt("123456789012345678901234567890")
big2 = BigInt(12345)  # From regular int

# Serialize
print(dumps(big))  # "123456789012345678901234567890n"

# Parse
result = loads("999999999999999999999999999999n")
print(type(result))  # <class 'kjson.types.BigInt'>
print(result.value)  # 999999999999999999999999999999
```

### Decimal128

High-precision decimal numbers:

```python
from kjson import Decimal128, dumps, loads

# Create Decimal128
price = Decimal128("99.99")
price2 = Decimal128(19.95)  # From float

# Serialize
print(dumps(price))  # "99.99m"

# Parse
result = loads("123.456789m")
print(type(result))  # <class 'kjson.types.Decimal128'>
print(str(result))   # "123.456789"
```

### UUID

Native UUID support with v4 and v7 generation:

```python
from kjson import uuid_v4, uuid_v7, dumps, loads
import uuid

# Generate UUIDs
id1 = uuid_v4()  # Random UUID v4
id2 = uuid_v7()  # Timestamp-based UUID v7

# Use existing UUID
id3 = uuid.UUID("550e8400-e29b-41d4-a716-446655440000")

# Serialize (unquoted)
print(dumps({"id": id1}))  # {id: 550e8400-e29b-41d4-a716-446655440000}

# Parse
result = loads("550e8400-e29b-41d4-a716-446655440000")
print(type(result))  # <class 'uuid.UUID'>
```

### Date

ISO 8601 date/time with timezone support:

```python
from kjson import Date, dumps, loads
from datetime import datetime, timezone

# Create Date
date1 = Date("2025-01-10T12:00:00Z")  # From ISO string
date2 = Date(datetime.now(timezone.utc))  # From datetime

# Serialize
print(dumps(date1))  # "2025-01-10T12:00:00Z"

# Parse
result = loads("2025-01-10T12:00:00-08:00")
print(type(result))  # <class 'kjson.types.Date'>
print(result.to_iso8601())  # "2025-01-10T12:00:00-08:00"
```

## JSON5 Features

### Comments

```python
data = loads('''
{
    // Single-line comment
    name: "example",
    /* Multi-line
       comment */
    value: 42
}
''')
```

### Unquoted Keys

```python
data = loads('{name: "John", age: 30}')
print(dumps(data))  # {name: "John", age: 30}
```

### Trailing Commas

```python
data = loads('''
{
    "items": [
        "first",
        "second",
    ],
}
''')
```

### Single Quotes

```python
data = loads("{'name': 'value'}")
```

## API Reference

### Core Functions

```python
# Parse kJSON text
loads(s: str, **kwargs) -> Any

# Serialize to kJSON
dumps(obj: Any, **kwargs) -> str
```

### Keyword Arguments

#### `dumps()` options:
- `indent`: int or str - Pretty-print indentation
- `sort_keys`: bool - Sort object keys alphabetically
- `ensure_ascii`: bool - Escape non-ASCII characters (default: False)
- `default`: callable - Function to convert unknown types

#### `loads()` options:
- `object_hook`: callable - Function called with each object

### Classes

```python
# Extended types
BigInt(value: Union[int, str])
Decimal128(value: Union[float, str])
Date(value: Union[datetime, str], tz_offset: Optional[int] = None)

# UUID generators
uuid_v4() -> uuid.UUID
uuid_v7() -> uuid.UUID

# Encoder/Decoder (for json module compatibility)
JSONEncoder(**kwargs)
JSONDecoder(**kwargs)
```

## Examples

### Financial Data

```python
from kjson import loads, dumps, BigInt, Decimal128, Date

transaction = {
    "id": "TX123456",
    "amount": Decimal128("9999.99"),
    "fee": Decimal128("0.01"),
    "gasUsed": BigInt("21000"),
    "blockNumber": BigInt("18500000"),
    "timestamp": Date("2025-01-10T15:30:00Z")
}

# Serialize with pretty printing
print(dumps(transaction, indent=2))
```

### Configuration Files

```python
config = loads('''
{
    // Server configuration
    server: {
        host: "localhost",
        port: 8080,
        // Development settings
        debug: true,
    },
    
    database: {
        url: "postgresql://localhost/myapp",
        pool_size: 10,
        timeout: 30,  // seconds
    },
}
''')
```

### Round-trip Serialization

```python
# Original data with extended types
original = {
    "id": uuid_v7(),
    "count": BigInt("999999999999999999"),
    "price": Decimal128("19.99"),
    "updated": Date(datetime.now(timezone.utc))
}

# Serialize to kJSON
kjson_str = dumps(original)

# Parse back
parsed = loads(kjson_str)

# Types are preserved
assert isinstance(parsed["id"], uuid.UUID)
assert isinstance(parsed["count"], BigInt)
assert isinstance(parsed["price"], Decimal128)
assert isinstance(parsed["updated"], Date)
```

## Compatibility

The kJSON Python client is designed as a drop-in replacement for Python's `json` module:

```python
# Replace this:
import json
data = json.loads(text)
output = json.dumps(data)

# With this:
import kjson
data = kjson.loads(text)  # Also handles extended types!
output = kjson.dumps(data)
```

## Error Handling

```python
from kjson import loads, JSONDecodeError

try:
    data = loads("{invalid json}")
except JSONDecodeError as e:
    print(f"Parse error at position {e.pos}: {e.msg}")
    print(f"Line {e.lineno}, column {e.colno}")
```

## Requirements

- Python 3.8 or higher
- No external dependencies (uses only Python standard library)

## Testing

```bash
# Run all tests
python -m pytest tests/

# Run specific test file
python -m pytest tests/test_parser.py -v

# Run with coverage
python -m pytest tests/ --cov=kjson
```

## License

MIT License - see LICENSE file in the repository root.