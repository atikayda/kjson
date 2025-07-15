import { assertEquals, assertRejects } from "https://deno.land/std@0.208.0/assert/mod.ts";
import { 
  parseStream, 
  stringifyToStream, 
  createParseTransform,
  createStringifyTransform,
  parseFile,
  writeFile,
  fromAsyncIterable
} from "../stream.ts";
import { UUID, Decimal128 } from "../mod.ts";

// Helper to create a readable stream from strings
function createReadableStream(chunks: string[]): ReadableStream<Uint8Array> {
  const encoder = new TextEncoder();
  return new ReadableStream({
    start(controller) {
      for (const chunk of chunks) {
        controller.enqueue(encoder.encode(chunk));
      }
      controller.close();
    }
  });
}

// Helper to collect stream output
async function collectStream(stream: ReadableStream<Uint8Array>): Promise<string> {
  const reader = stream.getReader();
  const decoder = new TextDecoder();
  let result = "";
  
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    result += decoder.decode(value);
  }
  
  return result;
}

Deno.test("parseStream - basic newline-delimited parsing", async () => {
  const input = [
    '{"name": "Alice", "age": 30}\n',
    '{id: 123n, active: true}\n',
    '{"value": 45.67m}\n'
  ];
  
  const stream = createReadableStream(input);
  const results: unknown[] = [];
  
  for await (const obj of parseStream(stream)) {
    results.push(obj);
  }
  
  assertEquals(results.length, 3);
  assertEquals(results[0], { name: "Alice", age: 30 });
  assertEquals(results[1], { id: 123n, active: true });
  assertEquals(results[2], { value: new Decimal128("45.67") });
});

Deno.test("parseStream - handles empty lines", async () => {
  const input = [
    '{"a": 1}\n',
    '\n',
    '  \n',
    '{"b": 2}\n',
    '\n'
  ];
  
  const stream = createReadableStream(input);
  const results: unknown[] = [];
  
  for await (const obj of parseStream(stream)) {
    results.push(obj);
  }
  
  assertEquals(results.length, 2);
  assertEquals(results[0], { a: 1 });
  assertEquals(results[1], { b: 2 });
});

Deno.test("parseStream - handles chunked input", async () => {
  const input = [
    '{"name": "Al',
    'ice", "age": 3',
    '0}\n{"id": 12',
    '3n}\n'
  ];
  
  const stream = createReadableStream(input);
  const results: unknown[] = [];
  
  for await (const obj of parseStream(stream)) {
    results.push(obj);
  }
  
  assertEquals(results.length, 2);
  assertEquals(results[0], { name: "Alice", age: 30 });
  assertEquals(results[1], { id: 123n });
});

Deno.test("parseStream - error handling with skipInvalid", async () => {
  const input = [
    '{"valid": true}\n',
    'invalid json\n',
    '{"another": "valid"}\n'
  ];
  
  const errors: Array<{ error: Error; line: string; lineNumber: number }> = [];
  const stream = createReadableStream(input);
  const results: unknown[] = [];
  
  for await (const obj of parseStream(stream, {
    skipInvalid: true,
    onError: (error, line, lineNumber) => {
      errors.push({ error, line, lineNumber });
    }
  })) {
    results.push(obj);
  }
  
  assertEquals(results.length, 2);
  assertEquals(results[0], { valid: true });
  assertEquals(results[1], { another: "valid" });
  assertEquals(errors.length, 1);
  assertEquals(errors[0].lineNumber, 2);
});

Deno.test("parseStream - throws on invalid without skipInvalid", async () => {
  const input = [
    '{"valid": true}\n',
    'invalid json\n'
  ];
  
  const stream = createReadableStream(input);
  
  await assertRejects(
    async () => {
      const results = [];
      for await (const obj of parseStream(stream)) {
        results.push(obj);
      }
    },
    Error,
    "Parse error on line 2"
  );
});

Deno.test("parseStream - handles last line without newline", async () => {
  const input = ['{"a": 1}\n{"b": 2}'];
  
  const stream = createReadableStream(input);
  const results: unknown[] = [];
  
  for await (const obj of parseStream(stream)) {
    results.push(obj);
  }
  
  assertEquals(results.length, 2);
  assertEquals(results[0], { a: 1 });
  assertEquals(results[1], { b: 2 });
});

Deno.test("stringifyToStream - basic streaming output", async () => {
  const chunks: Uint8Array[] = [];
  const stream = new WritableStream({
    write(chunk) {
      chunks.push(chunk);
    }
  });
  
  const writer = stringifyToStream(stream);
  
  await writer.write({ name: "Alice", age: 30 });
  await writer.write({ id: 123n, active: true });
  await writer.write(new Decimal128("45.67"));  // Direct Decimal128, not wrapped
  await writer.close();
  
  const decoder = new TextDecoder();
  const output = chunks.map(c => decoder.decode(c)).join("");
  
  const lines = output.split('\n').filter(l => l);
  assertEquals(lines.length, 3);
  assertEquals(lines[0], '{name:"Alice",age:30}');
  assertEquals(lines[1], '{id:123n,active:true}');
  assertEquals(lines[2], '45.67m');  // Direct Decimal128 value
});

Deno.test("stringifyToStream - custom delimiter", async () => {
  const chunks: Uint8Array[] = [];
  const stream = new WritableStream({
    write(chunk) {
      chunks.push(chunk);
    }
  });
  
  const writer = stringifyToStream(stream, { delimiter: '\r\n' });
  
  await writer.write({ a: 1 });
  await writer.write({ b: 2 });
  await writer.close();
  
  const decoder = new TextDecoder();
  const output = chunks.map(c => decoder.decode(c)).join("");
  
  assertEquals(output, '{a:1}\r\n{b:2}\r\n');
});

Deno.test("createParseTransform - transform stream", async () => {
  const input = ReadableStream.from([
    '{"a": 1}\n',
    '{"b": 2}\n'
  ]);
  
  const parsed = input.pipeThrough(createParseTransform());
  const results: unknown[] = [];
  
  const reader = parsed.getReader();
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    results.push(value);
  }
  
  assertEquals(results.length, 2);
  assertEquals(results[0], { a: 1 });
  assertEquals(results[1], { b: 2 });
});

Deno.test("createStringifyTransform - transform stream", async () => {
  const data = [
    { name: "Alice", id: UUID.v4() },
    { name: "Bob", amount: new Decimal128("99.99") }
  ];
  
  const stream = ReadableStream.from(data)
    .pipeThrough(createStringifyTransform({ space: 2 }));
  
  const results: string[] = [];
  const reader = stream.getReader();
  while (true) {
    const { done, value } = await reader.read();
    if (done) break;
    results.push(value);
  }
  
  assertEquals(results.length, 2);
  assertEquals(results[0].includes('name: "Alice"'), true);
  assertEquals(results[0].endsWith('\n'), true);
  assertEquals(results[1].includes('name: "Bob"'), true);
});

Deno.test("fromAsyncIterable - async generator to stream", async () => {
  async function* generateData() {
    yield { id: 1, value: "first" };
    yield { id: 2, value: "second" };
    yield { id: 3, value: "third" };
  }
  
  const stream = fromAsyncIterable(generateData())
    .pipeThrough(createStringifyTransform())
    .pipeThrough(new TextEncoderStream());
  
  const output = await collectStream(stream);
  const lines = output.split('\n').filter(l => l);
  
  assertEquals(lines.length, 3);
  assertEquals(lines[0], '{id:1,value:"first"}');
  assertEquals(lines[1], '{id:2,value:"second"}');
  assertEquals(lines[2], '{id:3,value:"third"}');
});

Deno.test("parseStream - handles UUID and Decimal types", async () => {
  const uuid = UUID.v4();
  const input = [
    `${uuid}\n`,  // Direct UUID
    '123.45m\n',  // Direct Decimal
    `{nested: {id: ${uuid}, amount: 67.89m}}\n`  // UUID and Decimal in object
  ];
  
  const stream = createReadableStream(input);
  const results: unknown[] = [];
  
  for await (const obj of parseStream(stream)) {
    results.push(obj);
  }
  
  assertEquals(results.length, 3);
  assertEquals(results[0] instanceof UUID, true);
  assertEquals((results[0] as UUID).toString(), uuid);
  assertEquals(results[1] instanceof Decimal128, true);
  assertEquals((results[1] as Decimal128).toString(), "123.45");
  
  const nested = results[2] as any;
  // Unquoted UUIDs should be parsed as UUID objects everywhere
  assertEquals(nested.nested.id instanceof UUID, true);
  assertEquals(nested.nested.id.toString(), uuid);
  assertEquals(nested.nested.amount instanceof Decimal128, true);
  assertEquals(nested.nested.amount.toString(), "67.89");
});

Deno.test("file operations - write and read", async () => {
  const tempFile = await Deno.makeTempFile({ suffix: ".kjsonl" });
  
  try {
    // Write data
    const data = [
      { id: UUID.v4(), name: "Test 1", value: 123n },
      { id: UUID.v4(), name: "Test 2", amount: new Decimal128("45.67") },
      { timestamp: new Date("2024-01-01"), items: [1, 2, 3] }
    ];
    
    await writeFile(tempFile, data);
    
    // Read data back
    const results: unknown[] = [];
    for await (const obj of parseFile(tempFile)) {
      results.push(obj);
    }
    
    assertEquals(results.length, 3);
    assertEquals((results[0] as any).value, 123n);
    assertEquals((results[1] as any).amount instanceof Decimal128, true);
    assertEquals((results[2] as any).timestamp instanceof Date, true);
  } finally {
    await Deno.remove(tempFile);
  }
});

Deno.test("parseStream - max line length protection", async () => {
  const longLine = '{"data": "' + 'x'.repeat(1000) + '"}\n';
  const input = [longLine];
  
  const stream = createReadableStream(input);
  
  await assertRejects(
    async () => {
      for await (const _obj of parseStream(stream, { maxLineLength: 100 })) {
        // Should not reach here
      }
    },
    Error,
    "exceeds maximum length"
  );
});

Deno.test("error recovery - continues after errors", async () => {
  const input = [
    '{"valid": 1}\n',
    'corrupted { data\n',
    '{"valid": 2}\n',
    '{ unterminated string: "\n',
    '{"valid": 3}\n'
  ];
  
  const stream = createReadableStream(input);
  const results: unknown[] = [];
  const errors: number[] = [];
  
  for await (const obj of parseStream(stream, {
    skipInvalid: true,
    onError: (_error, _line, lineNumber) => {
      errors.push(lineNumber);
    }
  })) {
    results.push(obj);
  }
  
  assertEquals(results.length, 3);
  assertEquals((results[0] as any).valid, 1);
  assertEquals((results[1] as any).valid, 2);
  assertEquals((results[2] as any).valid, 3);
  assertEquals(errors, [2, 4]);
});