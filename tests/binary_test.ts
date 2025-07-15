import { assertEquals, assertThrows } from "https://deno.land/std@0.208.0/assert/mod.ts";
import { 
  encode, 
  decode, 
  KJsonBType, 
  getType, 
  isValid,
  KJsonBEncoder,
  KJsonBDecoder
} from "../binary.ts";
import { UUID, Decimal128, stringify as kJsonStringify } from "../mod.ts";

Deno.test("kJSONB - null and boolean encoding", () => {
  assertEquals(decode(encode(null)), null);
  assertEquals(decode(encode(true)), true);
  assertEquals(decode(encode(false)), false);
  assertEquals(decode(encode(undefined)), undefined);
  
  // Check type bytes
  assertEquals(getType(encode(null)), KJsonBType.NULL);
  assertEquals(getType(encode(true)), KJsonBType.TRUE);
  assertEquals(getType(encode(false)), KJsonBType.FALSE);
  assertEquals(getType(encode(undefined)), KJsonBType.UNDEFINED);
});

Deno.test("kJSONB - number encoding", () => {
  // Small integers use efficient encoding
  const small = 42;
  const smallEncoded = encode(small);
  assertEquals(getType(smallEncoded), KJsonBType.INT8);
  assertEquals(decode(smallEncoded), small);
  
  // Medium integers
  const medium = 30000;
  const mediumEncoded = encode(medium);
  assertEquals(getType(mediumEncoded), KJsonBType.INT16);
  assertEquals(decode(mediumEncoded), medium);
  
  // Large integers
  const large = 1000000000;
  const largeEncoded = encode(large);
  assertEquals(getType(largeEncoded), KJsonBType.INT32);
  assertEquals(decode(largeEncoded), large);
  
  // Floating point
  const float = 3.14159;
  const floatEncoded = encode(float);
  assertEquals(getType(floatEncoded), KJsonBType.FLOAT64);
  assertEquals(decode(floatEncoded), float);
  
  // Special values
  assertEquals(decode(encode(NaN)), null);
  assertEquals(decode(encode(Infinity)), null);
  assertEquals(decode(encode(-Infinity)), null);
});

Deno.test("kJSONB - BigInt encoding", () => {
  const values = [
    0n,
    123n,
    -456n,
    123456789012345678901234567890n,
    -987654321098765432109876543210n
  ];
  
  for (const value of values) {
    const encoded = encode(value);
    assertEquals(getType(encoded), KJsonBType.BIGINT);
    assertEquals(decode(encoded), value);
  }
});

Deno.test("kJSONB - string encoding", () => {
  const strings = [
    "",
    "hello",
    "Hello, 世界! 🌍",
    "Special chars: \n\t\r\"'\\",
    "A".repeat(1000) // Long string
  ];
  
  for (const str of strings) {
    const encoded = encode(str);
    assertEquals(getType(encoded), KJsonBType.STRING);
    assertEquals(decode(encoded), str);
  }
});

Deno.test("kJSONB - Date encoding", () => {
  const dates = [
    new Date(),
    new Date("2024-01-01T00:00:00.000Z"),
    new Date(0), // Unix epoch
    new Date("1969-07-20T20:17:00.000Z") // Moon landing
  ];
  
  for (const date of dates) {
    const encoded = encode(date);
    assertEquals(getType(encoded), KJsonBType.DATE);
    const decoded = decode(encoded) as Date;
    assertEquals(decoded.getTime(), date.getTime());
  }
});

Deno.test("kJSONB - UUID encoding", () => {
  const uuids = [
    new UUID(UUID.v4()),
    new UUID(UUID.v7()),
    new UUID("550e8400-e29b-41d4-a716-446655440000")
  ];
  
  for (const uuid of uuids) {
    const encoded = encode(uuid);
    const typeValue = getType(encoded);
    assertEquals(typeValue, KJsonBType.UUID);
    const decoded = decode(encoded) as UUID;
    assertEquals(decoded.toString(), uuid.toString());
  }
});

Deno.test("kJSONB - Decimal128 encoding", () => {
  const decimals = [
    new Decimal128("0"),
    new Decimal128("123.45"),
    new Decimal128("-67.89"),
    new Decimal128("99999999999999999999999999999999.99")
  ];
  
  for (const decimal of decimals) {
    const encoded = encode(decimal);
    assertEquals(getType(encoded), KJsonBType.DECIMAL128);
    const decoded = decode(encoded) as Decimal128;
    assertEquals(decoded.toString(), decimal.toString());
  }
});

Deno.test("kJSONB - binary data encoding", () => {
  const binary = new Uint8Array([1, 2, 3, 4, 5, 255, 0, 127]);
  const encoded = encode(binary);
  assertEquals(getType(encoded), KJsonBType.BINARY);
  
  const decoded = decode(encoded) as Uint8Array;
  assertEquals(Array.from(decoded), Array.from(binary));
});

Deno.test("kJSONB - array encoding", () => {
  const arrays = [
    [],
    [1, 2, 3],
    ["a", "b", "c"],
    [null, true, false, 42, "hello"],
    [1n, 2n, 3n], // BigInt array
    [new Date(), new UUID(UUID.v4()), new Decimal128("123.45")], // Mixed types
    [[1, 2], [3, 4], [5, 6]] // Nested arrays
  ];
  
  for (const arr of arrays) {
    const encoded = encode(arr);
    assertEquals(getType(encoded), KJsonBType.ARRAY);
    const decoded = decode(encoded) as any[];
    
    assertEquals(decoded.length, arr.length);
    for (let i = 0; i < arr.length; i++) {
      if (arr[i] instanceof Date) {
        assertEquals((decoded[i] as Date).getTime(), (arr[i] as Date).getTime());
      } else if (arr[i] instanceof UUID) {
        assertEquals(decoded[i].toString(), arr[i]?.toString());
      } else if (arr[i] instanceof Decimal128) {
        assertEquals(decoded[i].toString(), arr[i]?.toString());
      } else {
        assertEquals(decoded[i], arr[i]);
      }
    }
  }
});

Deno.test("kJSONB - object encoding", () => {
  const objects = [
    {},
    { a: 1, b: 2, c: 3 },
    { name: "Alice", age: 30, active: true },
    { id: new UUID(UUID.v4()), amount: new Decimal128("99.99"), timestamp: new Date() },
    { nested: { deep: { value: 42 } } },
    { bigints: { a: 123n, b: 456n, c: 789n } }
  ];
  
  for (const obj of objects) {
    const encoded = encode(obj);
    assertEquals(getType(encoded), KJsonBType.OBJECT);
    const decoded = decode(encoded) as Record<string, any>;
    
    const keys = Object.keys(obj);
    assertEquals(Object.keys(decoded).sort(), keys.sort());
    
    for (const key of keys) {
      const original = (obj as any)[key];
      const decodedValue = decoded[key];
      
      if (original instanceof Date) {
        assertEquals(decodedValue.getTime(), original.getTime());
      } else if (original instanceof UUID) {
        assertEquals(decodedValue.toString(), original.toString());
      } else if (original instanceof Decimal128) {
        assertEquals(decodedValue.toString(), original.toString());
      } else if (typeof original === "object" && original !== null) {
        // For nested objects, compare directly since they may contain BigInt
        assertEquals(decodedValue, original);
      } else {
        assertEquals(decodedValue, original);
      }
    }
  }
});

Deno.test("kJSONB - complex nested structure", () => {
  const complex = {
    id: new UUID(UUID.v7()),
    metadata: {
      created: new Date(),
      version: 42n,
      tags: ["important", "urgent", "review"]
    },
    data: {
      items: [
        { name: "Item 1", price: new Decimal128("19.99"), inStock: true },
        { name: "Item 2", price: new Decimal128("29.99"), inStock: false }
      ],
      totals: {
        subtotal: new Decimal128("49.98"),
        tax: new Decimal128("4.50"),
        total: new Decimal128("54.48")
      }
    },
    binary: new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF])
  };
  
  const encoded = encode(complex);
  const decoded = decode(encoded) as any;
  
  assertEquals(decoded.id.toString(), complex.id.toString());
  assertEquals(decoded.metadata.created.getTime(), complex.metadata.created.getTime());
  assertEquals(decoded.metadata.version, complex.metadata.version);
  assertEquals(decoded.metadata.tags, complex.metadata.tags);
  assertEquals(decoded.data.items.length, 2);
  assertEquals(decoded.data.totals.total.toString(), "54.48");
  assertEquals(Array.from(decoded.binary), [0xDE, 0xAD, 0xBE, 0xEF]);
});

Deno.test("kJSONB - size efficiency", () => {
  // Compare binary size to kJSON string size (since JSON.stringify can't handle BigInt)
  const data = {
    numbers: Array.from({ length: 100 }, (_, i) => i),
    strings: Array.from({ length: 50 }, (_, i) => `string${i}`),
    dates: Array.from({ length: 20 }, () => new Date()),
    bigints: Array.from({ length: 30 }, (_, i) => BigInt(i * 1000000))
  };
  
  // Use kJSON stringify which handles BigInt
  const jsonSize = kJsonStringify(data).length;
  const binarySize = encode(data).length;
  
  console.log(`kJSON size: ${jsonSize} bytes`);
  console.log(`Binary size: ${binarySize} bytes`);
  console.log(`Compression ratio: ${(jsonSize / binarySize).toFixed(2)}x`);
  
  // Binary should be smaller for this numeric-heavy data
  assertEquals(binarySize < jsonSize, true);
});

Deno.test("kJSONB - isValid function", () => {
  assertEquals(isValid(encode({ a: 1 })), true);
  assertEquals(isValid(encode(null)), true);
  assertEquals(isValid(new Uint8Array([0xFF, 0xFF, 0xFF])), false);
  assertEquals(isValid(new Uint8Array([])), false);
});

Deno.test("kJSONB - error handling", () => {
  // Empty buffer
  assertThrows(() => decode(new Uint8Array([])), Error, "Unexpected end of buffer");
  
  // Invalid type byte
  assertThrows(() => decode(new Uint8Array([0xEE])), Error, "Unknown type byte");
  
  // Truncated data
  const validData = encode({ a: 1, b: 2 });
  assertThrows(
    () => decode(validData.slice(0, 3)), 
    Error, 
    "Unexpected end of buffer"
  );
});

Deno.test("kJSONB - round-trip consistency", () => {
  const testCases = [
    null,
    true,
    false,
    undefined,
    0,
    -42,
    3.14159,
    123n,
    -456789012345678901234567890n,
    "",
    "Hello, World!",
    new Date(),
    new UUID(UUID.v4()),
    new Decimal128("123.45"),
    [1, 2, 3],
    { a: 1, b: "two", c: true },
    new Uint8Array([1, 2, 3, 4, 5])
  ];
  
  for (const value of testCases) {
    const encoded = encode(value);
    const decoded = decode(encoded);
    
    if (value instanceof Date) {
      assertEquals((decoded as Date).getTime(), value.getTime());
    } else if (value instanceof UUID) {
      assertEquals((decoded as UUID).toString(), value.toString());
    } else if (value instanceof Decimal128) {
      assertEquals((decoded as Decimal128).toString(), value.toString());
    } else if (value instanceof Uint8Array) {
      assertEquals(Array.from(decoded as Uint8Array), Array.from(value));
    } else {
      assertEquals(decoded, value);
    }
  }
});