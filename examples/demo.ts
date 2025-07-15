#!/usr/bin/env deno run --allow-read --allow-write

/**
 * kJSON Demo - Showcasing Kind JSON Features
 * 
 * This demo demonstrates the key features of kJSON (Kind JSON):
 * - BigInt support with 'n' suffix
 * - Decimal128 support with 'm' suffix for precise decimals
 * - Date objects as ISO literals
 * - UUID support (unquoted)
 * - JSON5 syntax (unquoted keys, trailing commas, comments)
 * - Type preservation during parsing and stringification
 */

import { parse, stringify, kJSON } from "../mod.ts";
import { Decimal128 } from "../decimal128.ts";

console.log("🚀 kJSON - Kind JSON Demo\n");

// Sample data with all supported types
const testData = {
  // Regular JSON types
  name: "MT Trading Bot",
  version: 2.1,
  active: true,
  
  // BigInt values (perfect for blockchain slots, amounts, etc.)
  currentSlot: 250845123n,
  tokenAmount: 999999999999999999n,
  
  // Decimal128 values (perfect for prices, money calculations)
  price: new Decimal128("99.99"),
  fee: new Decimal128("0.25"),
  balance: new Decimal128("12345.67"),
  
  // UUID (unquoted in JSON)
  id: crypto.randomUUID(),
  
  // Date objects (automatically serialized as ISO literals)
  createdAt: new Date("2025-06-12T20:22:35.328Z"),
  lastUpdate: new Date(),
  
  // Nested structures
  config: {
    maxRetries: 5,
    timeout: 30000n,
    minPrice: new Decimal128("0.01"),
    endpoints: [
      "https://api.mainnet-beta.solana.com",
      "https://solana-api.projectserum.com"
    ]
  },
  
  // Arrays with mixed types
  recentSlots: [250845120n, 250845121n, 250845122n],
  priceHistory: [
    new Decimal128("98.50"),
    new Decimal128("99.00"),
    new Decimal128("99.99")
  ],
  
  // Strings that look like dates (should remain strings)
  notes: "Last processed: 2025-06-12T20:22:35.328Z"
};

console.log("📦 Original JavaScript object:");
console.log(testData);
console.log();

// Stringify with pretty printing
console.log("📝 kJSON representation:");
const kjsonString = stringify(testData, { space: 2 });
console.log(kjsonString);
console.log();

// Parse it back
console.log("🔄 Parsed back to JavaScript:");
const parsed = parse(kjsonString);
console.log(parsed);
console.log();

// Verify types are preserved
console.log("🔍 Type verification:");
console.log(`currentSlot type: ${typeof parsed.currentSlot} (${parsed.currentSlot})`);
console.log(`price type: ${parsed.price?.constructor?.name || typeof parsed.price} (${parsed.price})`);
console.log(`id type: ${typeof parsed.id} (UUID string)`);
console.log(`createdAt type: ${typeof parsed.createdAt} (${parsed.createdAt instanceof Date ? 'Date object' : 'not Date'})`);
console.log(`notes type: ${typeof parsed.notes} (string preserved)`);
console.log();

// Demonstrate JSON5 syntax features
console.log("✨ JSON5 syntax features demo:");

const json5Examples = [
  // Unquoted keys
  `{name: "test", value: 42}`,
  
  // Trailing commas
  `{a: 1, b: 2,}`,
  
  // Comments
  `{
    // This is a comment
    count: 100n, /* BigInt value */
    timestamp: 2025-06-12T20:22:35.328Z // Unquoted date literal
  }`,
  
  // Mixed quotes
  `{single: 'value', double: "value"}`,
  
  // BigInt, Decimal, and Date literals
  `{slot: 12345n, price: 99.99m, created: 2025-01-01T00:00:00.000Z}`
];

json5Examples.forEach((example, index) => {
  console.log(`Example ${index + 1}:`);
  console.log(`Input:  ${example.replace(/\n\s*/g, ' ')}`);
  try {
    const result = parse(example);
    console.log(`Output: ${JSON.stringify(result, (_, v) => typeof v === 'bigint' ? v.toString() + 'n' : v)}`);
    console.log(`✅ Parsed successfully\n`);
  } catch (error) {
    console.log(`❌ Error: ${error.message}\n`);
  }
});

// Demonstrate kJSON namespace utilities
console.log("🛠️ kJSON utility functions:");

// isValid function
console.log("kJSON.isValid tests:");
console.log(`  Valid JSON5: ${kJSON.isValid('{ name: "test", value: 42n }')}`);
console.log(`  Invalid JSON: ${kJSON.isValid('{ invalid json }')}`);
console.log();

// safeParseWith function
console.log("kJSON.safeParseWith tests:");
const safeResult1 = kJSON.safeParseWith('{ name: "test" }', { default: true });
console.log(`  Valid input: ${JSON.stringify(safeResult1)}`);

const safeResult2 = kJSON.safeParseWith('{ invalid }', { default: true });
console.log(`  Invalid input with default: ${JSON.stringify(safeResult2)}`);
console.log();

// Create custom parser
console.log("Custom parser example:");
const strictParser = kJSON.createParser({
  allowTrailingCommas: false,
  allowComments: false,
  allowUnquotedKeys: false
});

try {
  const strictResult = strictParser('{"name": "test", "value": 42}');
  console.log(`  Strict parser success: ${JSON.stringify(strictResult)}`);
} catch (error) {
  console.log(`  Strict parser error: ${error.message}`);
}
console.log();

// Create custom stringifier
console.log("Custom stringifier example:");
const prettyStringifier = kJSON.createStringifier({
  space: 2,
  quoteKeys: true,
  bigintSuffix: true
});

const prettyResult = prettyStringifier({ name: "test", count: 42n });
console.log("  Pretty formatted output:");
console.log(prettyResult);
console.log();

// Demonstrate Decimal128 arithmetic
console.log("💰 Decimal128 precise arithmetic:");
if (parsed.price instanceof Decimal128) {
  const price = parsed.price;
  const fee = parsed.fee;
  const total = price.add(fee);
  console.log(`Price: ${price.toString()}`);
  console.log(`Fee: ${fee.toString()}`);
  console.log(`Total: ${total.toString()} (exactly ${price.toString()} + ${fee.toString()})`);
  
  // Show why Decimal128 is important
  console.log("⚠️  JavaScript number precision problem:");
  console.log(`0.1 + 0.2 = ${0.1 + 0.2} (binary floating point error!)`);
  const d1 = new Decimal128("0.1");
  const d2 = new Decimal128("0.2");
  console.log(`Decimal128: 0.1 + 0.2 = ${d1.add(d2).toString()} (exact!)`);
} else {
  console.log("Note: Decimal128 support requires importing the Decimal128 class");
}
console.log();

// Performance comparison with standard JSON
console.log("⚡ Performance comparison:");

const iterations = 1000;
const testObject = {
  id: 12345n,
  timestamp: new Date(),
  data: Array.from({ length: 100 }, (_, i) => ({
    index: i,
    value: BigInt(i * 1000),
    created: new Date(Date.now() + i * 1000)
  }))
};

// kJSON
const startKjson = performance.now();
for (let i = 0; i < iterations; i++) {
  const str = stringify(testObject);
  parse(str);
}
const kjsonTime = performance.now() - startKjson;

// Standard JSON (for comparison, will lose BigInt precision)
const startJson = performance.now();
for (let i = 0; i < iterations; i++) {
  const str = JSON.stringify(testObject, (key, value) => 
    typeof value === 'bigint' ? value.toString() : value
  );
  JSON.parse(str);
}
const jsonTime = performance.now() - startJson;

console.log(`kJSON: ${kjsonTime.toFixed(2)}ms (${iterations} iterations)`);
console.log(`Standard JSON: ${jsonTime.toFixed(2)}ms (${iterations} iterations)`);
console.log(`Overhead: ${((kjsonTime / jsonTime - 1) * 100).toFixed(1)}%`);
console.log();

// Real-world use cases
console.log("🎯 Real-world use cases for kJSON:");

// Blockchain data example
const blockchainData = parse(`{
  // Solana block data
  slot: 250845123n,
  blockTime: 1703635200000n,
  blockHash: "ABC123...",
  
  transactions: [
    {
      signature: "XYZ789...",
      fee: 5000n,
      timestamp: 2025-01-01T00:00:00.000Z,
      accounts: ["wallet1", "wallet2"]
    }
  ],
  
  // Metadata
  processed: 2025-01-01T00:00:30.000Z,
  validator: "validator.example.com"
}`);

console.log("📊 Blockchain data parsing:");
console.log(`  Slot: ${blockchainData.slot} (${typeof blockchainData.slot})`);
console.log(`  Block time: ${blockchainData.blockTime} (${typeof blockchainData.blockTime})`);
console.log(`  Transaction fee: ${blockchainData.transactions[0].fee} (${typeof blockchainData.transactions[0].fee})`);
console.log(`  Timestamp: ${blockchainData.transactions[0].timestamp} (${blockchainData.transactions[0].timestamp instanceof Date ? 'Date' : 'not Date'})`);
console.log();

// Configuration file example
const configData = parse(`{
  // Database configuration
  database: {
    host: "localhost",
    port: 5432,
    maxConnections: 100n,
    timeout: 30000n,
  },
  
  // API settings
  api: {
    rateLimit: 1000n,
    timeout: 5000n,
    retries: 3,
  },
  
  // Feature flags
  features: {
    trading: true,
    analytics: false,
    // New feature coming soon
    advancedCharts: false,
  }
}`);

console.log("⚙️ Configuration file parsing:");
console.log(`  Max connections: ${configData.database.maxConnections} (${typeof configData.database.maxConnections})`);
console.log(`  Rate limit: ${configData.api.rateLimit} (${typeof configData.api.rateLimit})`);
console.log(`  Trading enabled: ${configData.features.trading}`);
console.log();

console.log("🌟 Key advantages of kJSON:");
console.log("- Type preservation for BigInt, Decimal128, Date, and UUID");
console.log("- Decimal128 for precise decimal arithmetic (no 0.1 + 0.2 errors!)");
console.log("- JSON5 syntax for more flexible configuration files");
console.log("- Comments support for self-documenting data");
console.log("- Perfect for blockchain and financial applications");
console.log("- Backward compatible with standard JSON");
console.log("- High performance with reasonable overhead");
console.log();

console.log("✅ Demo complete!");