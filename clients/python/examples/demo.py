#!/usr/bin/env python3
"""Demonstrate kJSON Python client features."""

import sys
import os
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from kjson import loads, dumps, BigInt, Decimal128, Date, uuid_v4, uuid_v7
from datetime import datetime, timezone


def main():
    """Run kJSON demonstration."""
    print("kJSON Python Client Demo")
    print("=" * 50)
    
    # Basic JSON
    print("\n1. Basic JSON:")
    basic_data = {
        "name": "kJSON Demo",
        "version": 1.0,
        "active": True,
        "tags": ["json", "extended", "python"]
    }
    basic_json = dumps(basic_data)
    print(f"Serialized: {basic_json}")
    print(f"Parsed back: {loads(basic_json)}")
    
    # Extended Types
    print("\n2. Extended Types:")
    
    # BigInt
    big_num = BigInt("123456789012345678901234567890")
    print(f"BigInt: {big_num} -> {dumps(big_num)}")
    
    # Decimal128
    price = Decimal128("99.99")
    print(f"Decimal128: {price} -> {dumps(price)}")
    
    # UUID
    id_v4 = uuid_v4()
    id_v7 = uuid_v7()
    print(f"UUID v4: {id_v4} -> {dumps(id_v4)}")
    print(f"UUID v7: {id_v7} -> {dumps(id_v7)}")
    
    # Date
    now = Date(datetime.now(timezone.utc))
    print(f"Date (UTC): {now} -> {dumps(now)}")
    
    # JSON5 Features
    print("\n3. JSON5 Features:")
    json5_str = """{
        // Configuration object
        name: 'My App',      // Single quotes work
        version: 1.0,        // Comments are preserved
        features: [
            'extended-types',
            'json5-syntax',
            'comments',      // Trailing comma is OK
        ],
        /* Multi-line comment
           explaining the config */
        settings: {
            debug: true,
        }
    }"""
    
    parsed = loads(json5_str)
    print(f"Parsed JSON5: {parsed}")
    
    # Complex Example
    print("\n4. Complex Example:")
    transaction = {
        "id": uuid_v7(),
        "timestamp": Date(datetime.now(timezone.utc)),
        "amount": Decimal128("1234.56"),
        "fee": Decimal128("0.01"),
        "blockNumber": BigInt("12345678901234567890"),
        "status": "confirmed",
        "metadata": {
            "network": "mainnet",
            "gasUsed": BigInt("21000"),
        }
    }
    
    # Pretty print
    pretty_json = dumps(transaction, indent=2)
    print("Pretty printed transaction:")
    print(pretty_json)
    
    # Round-trip test
    print("\n5. Round-trip Test:")
    parsed_transaction = loads(pretty_json)
    print(f"ID type: {type(parsed_transaction['id'])}")
    print(f"Timestamp type: {type(parsed_transaction['timestamp'])}")
    print(f"Amount type: {type(parsed_transaction['amount'])}")
    print(f"Block number type: {type(parsed_transaction['blockNumber'])}")
    
    # Error handling demo
    print("\n6. Error Handling:")
    try:
        loads("{invalid json}")
    except Exception as e:
        print(f"Parse error caught: {type(e).__name__}: {e}")
    
    print("\nDemo complete!")


if __name__ == "__main__":
    main()