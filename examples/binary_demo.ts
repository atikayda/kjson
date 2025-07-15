#!/usr/bin/env -S deno run

import { encode, decode } from "../binary.ts";
import { UUID, Decimal128, parse, stringify } from "../mod.ts";

console.log("=== kJSONB Binary Format Demo ===\n");

// Demo 1: Basic types
console.log("1. Basic Types:");
const basicData = {
  name: "Alice",
  age: 30,
  active: true,
  balance: null
};

const basicBinary = encode(basicData);
const basicDecoded = decode(basicBinary);

console.log("Original:", basicData);
console.log("Binary size:", basicBinary.length, "bytes");
console.log("JSON size:", JSON.stringify(basicData).length, "bytes");
console.log("Decoded:", basicDecoded);
console.log();

// Demo 2: Extended types
console.log("2. Extended Types:");
const extendedData = {
  id: new UUID(UUID.v7()),
  amount: new Decimal128("12345.67"),
  timestamp: new Date(),
  bigNumber: 123456789012345678901234567890n
};

const extendedBinary = encode(extendedData);
const extendedDecoded = decode(extendedBinary);

console.log("Original:");
console.log("  ID:", extendedData.id.toString());
console.log("  Amount:", extendedData.amount.toString());
console.log("  Timestamp:", extendedData.timestamp.toISOString());
console.log("  BigNumber:", extendedData.bigNumber.toString());

console.log("\nBinary size:", extendedBinary.length, "bytes");
console.log("kJSON size:", stringify(extendedData).length, "bytes");

console.log("\nDecoded:");
console.log("  ID:", (extendedDecoded as any).id.toString());
console.log("  Amount:", (extendedDecoded as any).amount.toString());
console.log("  Timestamp:", (extendedDecoded as any).timestamp.toISOString());
console.log("  BigNumber:", (extendedDecoded as any).bigNumber.toString());
console.log();

// Demo 3: Complex nested structure
console.log("3. Complex Structure:");
const complexData = {
  users: [
    {
      id: new UUID(UUID.v4()),
      name: "Bob",
      transactions: [
        { amount: new Decimal128("100.50"), date: new Date("2024-01-01") },
        { amount: new Decimal128("250.75"), date: new Date("2024-01-02") }
      ]
    },
    {
      id: new UUID(UUID.v4()),
      name: "Charlie",
      transactions: [
        { amount: new Decimal128("75.00"), date: new Date("2024-01-03") }
      ]
    }
  ],
  metadata: {
    version: 1n,
    created: new Date(),
    tags: ["production", "finance", "v2"]
  }
};

const complexBinary = encode(complexData);
const complexDecoded = decode(complexBinary) as any;

console.log("Binary size:", complexBinary.length, "bytes");
console.log("kJSON size:", stringify(complexData).length, "bytes");
console.log("Compression ratio:", (stringify(complexData).length / complexBinary.length).toFixed(2) + "x");

console.log("\nDecoded structure verified:");
console.log("  Users count:", complexDecoded.users.length);
console.log("  First user name:", complexDecoded.users[0].name);
console.log("  First user ID type:", complexDecoded.users[0].id.constructor.name);
console.log("  Transaction amount type:", complexDecoded.users[0].transactions[0].amount.constructor.name);
console.log("  Metadata version type:", typeof complexDecoded.metadata.version);
console.log();

// Demo 4: Binary data
console.log("4. Binary Data:");
const binaryData = {
  file: "image.png",
  content: new Uint8Array([0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10]), // JPEG header
  checksum: 0xDEADBEEFn
};

const binaryEncoded = encode(binaryData);
const binaryDecoded = decode(binaryEncoded) as any;

console.log("Original binary:", Array.from(binaryData.content));
console.log("Decoded binary:", Array.from(binaryDecoded.content));
console.log("Checksum preserved:", binaryDecoded.checksum === binaryData.checksum);
console.log();

// Demo 5: Size comparison
console.log("5. Size Comparison (Numeric Data):");
const numericData = {
  integers: Array.from({ length: 100 }, (_, i) => i),
  bigints: Array.from({ length: 50 }, (_, i) => BigInt(i * 1000000)),
  decimals: Array.from({ length: 30 }, (_, i) => new Decimal128(`${i}.${i}5`)),
  dates: Array.from({ length: 20 }, () => new Date())
};

const numericBinary = encode(numericData);
const numericKJson = stringify(numericData);

console.log("Binary size:", numericBinary.length, "bytes");
console.log("kJSON size:", numericKJson.length, "bytes");
console.log("Space saved:", ((1 - numericBinary.length / numericKJson.length) * 100).toFixed(1) + "%");
console.log();

// Demo 6: Type preservation
console.log("6. Type Preservation:");
const typeTest = [
  42,                              // number
  42n,                             // bigint
  new Decimal128("42.00"),         // decimal
  "42",                            // string
  new Date(42),                    // date
  new UUID("00000000-0000-0000-0000-000000000042") // uuid
];

const typeEncoded = encode(typeTest);
const typeDecoded = decode(typeEncoded) as any[];

console.log("Type preservation check:");
typeDecoded.forEach((value, i) => {
  const type = value.constructor.name;
  const original = typeTest[i].constructor.name;
  console.log(`  [${i}] ${original} -> ${type}: ${type === original ? '✓' : '✗'}`);
});

console.log("\n=== Demo Complete ===");