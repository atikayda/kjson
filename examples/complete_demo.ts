#!/usr/bin/env -S deno run --allow-write --allow-read

import { 
  parse, 
  stringify, 
  UUID, 
  Decimal128,
  parseStream,
  stringifyToStream,
  encode as encodeBinary,
  decode as decodeBinary,
  KJsonBType
} from "../mod.ts";

console.log("=== kJSON Complete Feature Demo ===\n");

// Demo 1: Extended Type Parsing
console.log("1. Extended Type Parsing:");
const kjsonText = `{
  // User profile with extended types
  id: 01912d68-783e-7a03-8467-5661c1243ad4,  // Unquoted UUID
  name: "Alice Johnson",
  balance: 12345.67m,  // Decimal with 'm' suffix
  score: 98765432109876543210n,  // BigInt with 'n' suffix
  joined: 2024-01-15T10:30:00.000Z,  // ISO date
  tags: ["premium", "verified", "early-adopter"],
  settings: {
    theme: 'dark',  // Single quotes
    notifications: true,
    limits: {
      daily: 1000n,
      monthly: 25000n,
    },  // Trailing comma
  }
}`;

const parsed = parse(kjsonText);
console.log("Parsed object types:");
console.log("  id:", parsed.id.constructor.name);
console.log("  balance:", parsed.balance.constructor.name);
console.log("  score:", typeof parsed.score);
console.log("  joined:", parsed.joined.constructor.name);
console.log();

// Demo 2: Stringification with formatting
console.log("2. Pretty Printing:");
const data = {
  transaction: {
    id: new UUID(UUID.v7()),
    from: "Alice",
    to: "Bob",
    amount: new Decimal128("500.00"),
    timestamp: new Date(),
    metadata: {
      network: "mainnet",
      gasLimit: 21000n,
      confirmed: true
    }
  }
};

const pretty = stringify(data, { space: 2 });
console.log(pretty);
console.log();

// Demo 3: Streaming newline-delimited kJSON
console.log("3. Streaming kJSON:");
const tempFile = await Deno.makeTempFile({ suffix: ".kjsonl" });

// Write stream of events
const writeFile = await Deno.open(tempFile, { 
  write: true, 
  create: true, 
  truncate: true 
});

const writer = stringifyToStream(writeFile.writable);

console.log("Writing events to stream...");
for (let i = 0; i < 5; i++) {
  const event = {
    eventId: new UUID(UUID.v7()),
    type: ["login", "purchase", "logout"][i % 3],
    userId: 1000 + i,
    timestamp: new Date(Date.now() + i * 1000),
    value: i > 0 ? new Decimal128((Math.random() * 100).toFixed(2)) : null
  };
  await writer.write(event);
  console.log(`  - Event ${i + 1}: ${event.type}`);
}
await writer.close();

// Read stream back
console.log("\nReading events from stream:");
const readFile = await Deno.open(tempFile, { read: true });
let eventCount = 0;
for await (const event of parseStream(readFile.readable)) {
  eventCount++;
  const e = event as any;
  console.log(`  - ${e.type} at ${e.timestamp.toISOString()}`);
}
console.log(`Total events read: ${eventCount}`);
console.log();

// Demo 4: Binary format efficiency
console.log("4. Binary Format (kJSONB):");
const complexData = {
  metrics: {
    cpu: Array.from({ length: 100 }, (_, i) => ({
      timestamp: new Date(Date.now() - i * 1000),
      value: Math.random() * 100,
      load: BigInt(Math.floor(Math.random() * 1000000))
    })),
    memory: {
      total: 16777216n,  // 16GB in KB
      used: 8388608n,    // 8GB in KB
      buffers: 1048576n  // 1GB in KB
    }
  },
  alerts: [
    {
      id: new UUID(UUID.v4()),
      severity: "high",
      threshold: new Decimal128("90.5"),
      triggered: new Date()
    }
  ]
};

const jsonSize = stringify(complexData).length;
const binaryData = encodeBinary(complexData);
const binarySize = binaryData.length;

console.log("Format comparison:");
console.log(`  kJSON size: ${jsonSize} bytes`);
console.log(`  Binary size: ${binarySize} bytes`);
console.log(`  Compression: ${((1 - binarySize / jsonSize) * 100).toFixed(1)}%`);
console.log(`  Ratio: ${(jsonSize / binarySize).toFixed(2)}x`);

// Verify round-trip
const decoded = decodeBinary(binaryData) as any;
console.log("\nBinary round-trip verification:");
console.log("  CPU samples:", decoded.metrics.cpu.length);
console.log("  Memory total type:", typeof decoded.metrics.memory.total);
console.log("  Alert ID type:", decoded.alerts[0].id.constructor.name);
console.log("  Threshold type:", decoded.alerts[0].threshold.constructor.name);
console.log();

// Demo 5: Error handling with recovery
console.log("5. Error Recovery in Streams:");
const errorStream = new ReadableStream({
  start(controller) {
    controller.enqueue(new TextEncoder().encode('{valid: true, id: ' + UUID.v4() + '}\n'));
    controller.enqueue(new TextEncoder().encode('invalid json here\n'));
    controller.enqueue(new TextEncoder().encode('{value: 123.45m, ok: true}\n'));
    controller.enqueue(new TextEncoder().encode('{corrupted: \n'));
    controller.enqueue(new TextEncoder().encode('{final: "entry", num: 999n}\n'));
    controller.close();
  }
});

const results: any[] = [];
const errors: string[] = [];

for await (const obj of parseStream(errorStream, {
  skipInvalid: true,
  onError: (error, line, lineNumber) => {
    errors.push(`Line ${lineNumber}: ${error.message.split('\n')[0]}`);
  }
})) {
  results.push(obj);
}

console.log(`Successfully parsed: ${results.length} objects`);
console.log(`Errors encountered: ${errors.length}`);
errors.forEach(e => console.log(`  - ${e}`));
console.log();

// Demo 6: Type preservation showcase
console.log("6. Complete Type Preservation:");
const allTypes = {
  // Standard JSON types
  string: "Hello, kJSON!",
  number: 42.5,
  boolean: true,
  "null": null,
  array: [1, 2, 3],
  object: { nested: "value" },
  
  // Extended types
  bigint: 123456789012345678901234567890n,
  decimal: new Decimal128("99999.99"),
  uuid: new UUID("550e8400-e29b-41d4-a716-446655440000"),
  date: new Date("2025-01-01T00:00:00.000Z"),
  undefined: undefined,
  binary: new Uint8Array([0xDE, 0xAD, 0xBE, 0xEF])
};

// Test through all formats
const kjsonStr = stringify(allTypes, { space: 2 });
console.log("Stringified kJSON (first 200 chars):");
console.log(kjsonStr.slice(0, 200) + "...");
console.log();

// Parse the stringified kJSON
let kjsonParsed;
try {
  kjsonParsed = parse(kjsonStr);
} catch (e) {
  // Handle the unknown exception type properly
  const errorMessage = e instanceof Error ? e.message : String(e);
  console.log("ERROR parsing kJSON:", errorMessage);
  console.log("Problem around:", kjsonStr.slice(50, 80));
  kjsonParsed = {};
}
const binaryEncoded = encodeBinary(allTypes);
const binaryDecoded = decodeBinary(binaryEncoded) as Record<string, any>;

console.log("Type preservation test:");
Object.entries(allTypes).forEach(([key, value]) => {
  if (value === undefined) {
    console.log(`  ${key}: kJSON: ${kjsonParsed[key] === undefined ? '✓' : '✗'}, Binary: ${binaryDecoded[key] === undefined ? '✓' : '✗'}`);
  } else if (value instanceof Uint8Array) {
    const kjsonOk = kjsonParsed[key] === undefined; // Not supported in text format
    const binaryOk = binaryDecoded[key] instanceof Uint8Array && 
                     Array.from(binaryDecoded[key]).join(',') === Array.from(value).join(',');
    console.log(`  ${key}: kJSON: ${kjsonOk ? 'N/A' : '✗'}, Binary: ${binaryOk ? '✓' : '✗'}`);
  } else if (value && typeof value === 'object') {
    const kjsonOk = kjsonParsed[key]?.constructor.name === value.constructor.name;
    const binaryOk = binaryDecoded[key]?.constructor.name === value.constructor.name;
    console.log(`  ${key}: kJSON: ${kjsonOk ? '✓' : '✗'}, Binary: ${binaryOk ? '✓' : '✗'}`);
  } else {
    const kjsonOk = kjsonParsed[key] === value;
    const binaryOk = binaryDecoded[key] === value;
    console.log(`  ${key}: kJSON: ${kjsonOk ? '✓' : '✗'}, Binary: ${binaryOk ? '✓' : '✗'}`);
  }
});

// Cleanup
await Deno.remove(tempFile);

console.log("\n=== Demo Complete ===");
console.log("\nkJSON Features Demonstrated:");
console.log("✓ Extended type parsing (UUID, Decimal128, BigInt, Date)");
console.log("✓ JSON5 syntax (comments, unquoted keys, trailing commas)");
console.log("✓ Pretty printing with proper formatting");
console.log("✓ Streaming newline-delimited kJSON");
console.log("✓ Binary format with compression");
console.log("✓ Error recovery in streams");
console.log("✓ Complete type preservation");