#!/usr/bin/env -S deno run --allow-write --allow-read

import { 
  UUID, 
  Decimal128, 
  parseStream, 
  stringifyToStream,
  createStringifyTransform,
  fromAsyncIterable
} from "../mod.ts";

console.log("=== kJSON Streaming Demo ===\n");

// Demo 1: Create a stream of trading data
console.log("1. Generating trading data stream...");

async function* generateTradingData() {
  for (let i = 0; i < 5; i++) {
    yield {
      id: UUID.v7(),  // Time-ordered UUID
      timestamp: new Date(),
      symbol: ["BTC", "ETH", "SOL", "DOGE"][Math.floor(Math.random() * 4)],
      price: new Decimal128((Math.random() * 1000).toFixed(2)),
      volume: BigInt(Math.floor(Math.random() * 1000000)),
      metadata: {
        exchange: "Binance",
        orderId: UUID.v4()
      }
    };
    
    // Simulate real-time data
    await new Promise(resolve => setTimeout(resolve, 100));
  }
}

// Write to a file
const outputFile = "trades.kjsonl";
console.log(`\nWriting to ${outputFile}...`);

const file = await Deno.open(outputFile, { 
  write: true, 
  create: true, 
  truncate: true 
});

const writer = stringifyToStream(file.writable, { space: 0 });

for await (const trade of generateTradingData()) {
  console.log(`- Trade ${trade.symbol}: $${trade.price.toString()}`);
  await writer.write(trade);
}

await writer.close();

// Demo 2: Read and process the stream
console.log("\n2. Reading and processing the stream...");

const readFile = await Deno.open(outputFile, { read: true });
const totals = new Map<string, { count: number; volume: bigint }>();

for await (const trade of parseStream(readFile.readable)) {
  const t = trade as any;
  
  if (!totals.has(t.symbol)) {
    totals.set(t.symbol, { count: 0, volume: 0n });
  }
  
  const stats = totals.get(t.symbol)!;
  stats.count++;
  stats.volume += t.volume;
}

console.log("\nTrade Summary:");
for (const [symbol, stats] of totals) {
  console.log(`- ${symbol}: ${stats.count} trades, volume: ${stats.volume.toLocaleString()}`);
}

// Demo 3: Stream transformation pipeline
console.log("\n3. Stream transformation pipeline...");

// Create a pipeline that enriches data
const enrichedStream = fromAsyncIterable(generateTradingData())
  .pipeThrough(new TransformStream({
    transform(trade: any, controller) {
      // Add analysis
      trade.analysis = {
        priceLevel: trade.price.toNumber() > 500 ? "high" : "low",
        volumeLevel: trade.volume > 500000n ? "high" : "low",
        alert: trade.volume > 800000n && trade.price.toNumber() > 700
      };
      controller.enqueue(trade);
    }
  }))
  .pipeThrough(createStringifyTransform({ space: 2 }));

console.log("\nEnriched trades (showing first 2):");
const reader = enrichedStream.getReader();
let count = 0;
while (count < 2) {
  const { done, value } = await reader.read();
  if (done) break;
  console.log(value);
  count++;
}
reader.releaseLock();

// Demo 4: Error handling
console.log("\n4. Error handling demo...");

const mixedData = [
  '{"valid": true, "id": ' + UUID.v4() + '}',
  'invalid json here',
  '{number: 123.45m}',
  '{"corrupted": ',
  `{timestamp: ${new Date().toISOString()}, value: 999n}`
];

const errorStream = new ReadableStream({
  start(controller) {
    for (const line of mixedData) {
      controller.enqueue(new TextEncoder().encode(line + '\n'));
    }
    controller.close();
  }
});

console.log("\nProcessing stream with errors (skipInvalid=true):");
const errors: string[] = [];
let successCount = 0;

for await (const obj of parseStream(errorStream, {
  skipInvalid: true,
  onError: (error, line, lineNumber) => {
    errors.push(`Line ${lineNumber}: ${error.message}`);
  }
})) {
  successCount++;
  console.log(`✓ Parsed:`, obj);
}

console.log(`\nSummary: ${successCount} successful, ${errors.length} errors`);
errors.forEach(e => console.log(`✗ ${e}`));

// Cleanup
await Deno.remove(outputFile);
console.log(`\nDemo complete! Cleaned up ${outputFile}`);