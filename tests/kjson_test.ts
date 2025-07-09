import { assertEquals, assertThrows, assertStrictEquals } from "@std/assert";
import { parse, stringify, kJSON } from "../mod.ts";

// Test cases for comprehensive kJSON functionality
Deno.test("kJSON - Basic JSON compatibility", () => {
  const input = { name: "test", value: 42, nested: { x: 1, y: 2 } };
  const json = stringify(input);
  const parsed = parse(json);
  
  assertEquals(parsed, input);
});

Deno.test("kJSON - BigInt support", () => {
  const input = { 
    small: 42n, 
    large: 999999999999999999n,
    negative: -123456789n 
  };
  
  const json = stringify(input);
  const parsed = parse(json);
  
  assertEquals(typeof parsed.small, "bigint");
  assertEquals(parsed.small, 42n);
  assertEquals(parsed.large, 999999999999999999n);
  assertEquals(parsed.negative, -123456789n);
});

Deno.test("kJSON - BigInt in arrays", () => {
  const input = [1n, 2n, 3n];
  const json = stringify(input);
  const parsed = parse(json);
  
  assertEquals(parsed.length, 3);
  assertEquals(typeof parsed[0], "bigint");
  assertEquals(parsed[0], 1n);
  assertEquals(parsed[1], 2n);
  assertEquals(parsed[2], 3n);
});

Deno.test("kJSON - Date support", () => {
  const date = new Date("2025-06-12T20:22:35.328Z");
  const input = {
    created: date,
    updated: new Date("2025-01-01T00:00:00.000Z")
  };
  
  const json = stringify(input);
  const parsed = parse(json);
  
  assertEquals(parsed.created instanceof Date, true);
  assertEquals(parsed.created.getTime(), date.getTime());
  assertEquals(parsed.updated instanceof Date, true);
});

Deno.test("kJSON - Date vs string distinction", () => {
  const actualDate = new Date("2025-06-12T20:22:35.328Z");
  const input = {
    actualDate: actualDate,
    dateString: "2025-06-12T20:22:35.328Z",
    description: "This is a date: 2025-06-12T20:22:35.328Z"
  };
  
  const json = stringify(input);
  const parsed = parse(json);
  
  assertEquals(parsed.actualDate instanceof Date, true);
  assertEquals(typeof parsed.dateString, "string");
  assertEquals(typeof parsed.description, "string");
});

Deno.test("kJSON - Mixed BigInt and numbers", () => {
  const input = { 
    regular: 42, 
    big: 42n,
    float: 3.14,
    scientific: 1.23e-4
  };
  
  const json = stringify(input);
  const parsed = parse(json);
  
  assertEquals(typeof parsed.regular, "number");
  assertEquals(typeof parsed.big, "bigint");
  assertEquals(typeof parsed.float, "number");
  assertEquals(typeof parsed.scientific, "number");
});

Deno.test("kJSON - Unquoted object keys", () => {
  const jsonStr = '{name: "test", camelCase: "value", snake_case: "value2"}';
  const parsed = parse(jsonStr);
  
  assertEquals(parsed.name, "test");
  assertEquals(parsed.camelCase, "value");
  assertEquals(parsed.snake_case, "value2");
});

Deno.test("kJSON - Keys requiring quotes", () => {
  const input = { "with-dash": "value", "123numeric": "value2", "with space": "value3" };
  const json = stringify(input);
  const parsed = parse(json);
  
  assertEquals(parsed["with-dash"], "value");
  assertEquals(parsed["123numeric"], "value2");
  assertEquals(parsed["with space"], "value3");
});

Deno.test("kJSON - Trailing commas", () => {
  const jsonStr = `{
    a: 1,
    b: 2,
    c: [1, 2, 3,],
  }`;
  
  const parsed = parse(jsonStr);
  assertEquals(parsed.a, 1);
  assertEquals(parsed.b, 2);
  assertEquals(parsed.c, [1, 2, 3]);
});

Deno.test("kJSON - Single-line comments", () => {
  const jsonStr = `{
    // This is a name
    name: "test",
    value: 42 // This is a value
  }`;
  
  const parsed = parse(jsonStr);
  assertEquals(parsed.name, "test");
  assertEquals(parsed.value, 42);
});

Deno.test("kJSON - Multi-line comments", () => {
  const jsonStr = `{
    /* Multi-line
       comment here */
    a: 1,
    b: 2 /* inline block comment */
  }`;
  
  const parsed = parse(jsonStr);
  assertEquals(parsed.a, 1);
  assertEquals(parsed.b, 2);
});

Deno.test("kJSON - Mixed quote styles", () => {
  const jsonStr = `{single: 'value', double: "value2"}`;
  const parsed = parse(jsonStr);
  
  assertEquals(parsed.single, "value");
  assertEquals(parsed.double, "value2");
});

Deno.test("kJSON - Special values", () => {
  const jsonStr = '{nothing: null, yes: true, no: false}';
  const parsed = parse(jsonStr);
  
  assertEquals(parsed.nothing, null);
  assertEquals(parsed.yes, true);
  assertEquals(parsed.no, false);
});

Deno.test("kJSON - Empty structures", () => {
  const jsonStr = '{empty_obj: {}, empty_arr: []}';
  const parsed = parse(jsonStr);
  
  assertEquals(parsed.empty_obj, {});
  assertEquals(parsed.empty_arr, []);
});

Deno.test("kJSON - String escapes", () => {
  const input = { 
    newline: "line1\nline2",
    tab: "col1\tcol2", 
    quote: 'He said "hello"',
    backslash: "path\\to\\file"
  };
  
  const json = stringify(input);
  const parsed = parse(json);
  
  assertEquals(parsed.newline, "line1\nline2");
  assertEquals(parsed.tab, "col1\tcol2");
  assertEquals(parsed.quote, 'He said "hello"');
  assertEquals(parsed.backslash, "path\\to\\file");
});

Deno.test("kJSON - Complex nested with BigInt", () => {
  const input = {
    metadata: {
      id: 12345n,
      timestamp: 1640995200000n,
      version: "1.0"
    },
    data: [
      { slot: 100n, amount: 999999999999999999n },
      { slot: 101n, amount: 888888888888888888n }
    ],
    config: {
      enabled: true,
      maxRetries: 3
    }
  };
  
  const json = stringify(input);
  const parsed = parse(json);
  
  assertEquals(typeof parsed.metadata.id, "bigint");
  assertEquals(parsed.metadata.id, 12345n);
  assertEquals(typeof parsed.data[0].slot, "bigint");
  assertEquals(parsed.data[0].amount, 999999999999999999n);
  assertEquals(parsed.config.enabled, true);
});

Deno.test("kJSON - Pretty printing with spaces", () => {
  const input = { a: 1, b: { c: 2 } };
  const json = stringify(input, { space: 2 });
  
  const parsed = parse(json);
  assertEquals(parsed, input);
});

Deno.test("kJSON - Force quote all keys", () => {
  const input = { name: "test", value: 42 };
  const json = stringify(input, { quoteKeys: true });
  
  const parsed = parse(json);
  assertEquals(parsed, input);
});

Deno.test("kJSON - BigInt without suffix", () => {
  const input = { count: 123n };
  const json = stringify(input, { bigintSuffix: false });
  
  // Without the 'n' suffix, it becomes a regular number when parsed
  const parsed = parse(json);
  assertEquals(typeof parsed.count, "number");
  assertEquals(parsed.count, 123);
});

Deno.test("kJSON - Error: Invalid JSON missing comma", () => {
  assertThrows(() => {
    parse('{a: 1 b: 2}');
  }, Error, "Expected ',' or '}'");
});

Deno.test("kJSON - Error: Trailing comma when disabled", () => {
  assertThrows(() => {
    parse('{a: 1, b: 2,}', { allowTrailingCommas: false });
  }, Error, "Trailing comma not allowed");
});

Deno.test("kJSON - Error: Comments when disabled", () => {
  assertThrows(() => {
    parse('{// comment\na: 1}', { allowComments: false });
  }, Error, "Unexpected character");
});

Deno.test("kJSON - Error: Unquoted key when disabled", () => {
  assertThrows(() => {
    parse('{key: "value"}', { allowUnquotedKeys: false });
  }, Error, "Expected string");
});

Deno.test("kJSON - Error: Unterminated string", () => {
  assertThrows(() => {
    parse('{"unterminated": "string');
  }, Error, "Unterminated string");
});

Deno.test("kJSON - kJSON.isValid", () => {
  assertEquals(kJSON.isValid('{ name: "test" }'), true);
  assertEquals(kJSON.isValid('{ invalid json }'), false);
  assertEquals(kJSON.isValid('{ value: 42n }'), true);
  assertEquals(kJSON.isValid('{ date: 2025-01-01T00:00:00.000Z }'), true);
});

Deno.test("kJSON - kJSON.safeParseWith", () => {
  const result1 = kJSON.safeParseWith('{ name: "test" }', { default: true });
  assertEquals((result1 as any).name, "test");
  
  const result2 = kJSON.safeParseWith('{ invalid }', { default: true });
  assertEquals((result2 as any).default, true);
});

Deno.test("kJSON - kJSON.createParser", () => {
  const strictParser = kJSON.createParser({
    allowTrailingCommas: false,
    allowComments: false,
    allowUnquotedKeys: false
  });
  
  const result = strictParser('{"name": "test"}');
  assertEquals(result.name, "test");
  
  assertThrows(() => {
    strictParser('{name: "test"}');
  });
});

Deno.test("kJSON - kJSON.createStringifier", () => {
  const prettyStringifier = kJSON.createStringifier({
    space: 2,
    quoteKeys: true
  });
  
  const json = prettyStringifier({ name: "test", value: 42n });
  const parsed = parse(json);
  
  assertEquals(parsed.name, "test");
  assertEquals(typeof parsed.value, "bigint");
});

Deno.test("kJSON - Round-trip consistency", () => {
  const original = {
    str: "hello",
    num: 42,
    big: 123456789012345678901234567890n,
    date: new Date("2025-01-01T00:00:00.000Z"),
    bool: true,
    nil: null,
    obj: { nested: "value" },
    arr: [1, 2n, "three"]
  };
  
  const json = stringify(original);
  const parsed = parse(json);
  
  assertEquals(typeof parsed.str, "string");
  assertEquals(typeof parsed.num, "number");
  assertEquals(typeof parsed.big, "bigint");
  assertEquals(parsed.date instanceof Date, true);
  assertEquals(typeof parsed.bool, "boolean");
  assertEquals(parsed.nil, null);
  assertEquals(typeof parsed.obj, "object");
  assertEquals(Array.isArray(parsed.arr), true);
  assertEquals(typeof parsed.arr[1], "bigint");
});

Deno.test("kJSON - Performance baseline", () => {
  const testData = {
    id: 12345678901234567890n,
    timestamp: new Date(),
    data: Array.from({ length: 100 }, (_, i) => ({
      index: i,
      value: BigInt(i * 1000),
      active: i % 2 === 0
    }))
  };
  
  const start = performance.now();
  const json = stringify(testData);
  const parsed = parse(json);
  const end = performance.now();
  
  // Should complete in reasonable time (< 100ms for this dataset)
  const duration = end - start;
  console.log(`Performance test completed in ${duration.toFixed(2)}ms`);
  
  // Verify correctness
  assertEquals(typeof parsed.id, "bigint");
  assertEquals(parsed.timestamp instanceof Date, true);
  assertEquals(parsed.data.length, 100);
  assertEquals(typeof parsed.data[0].value, "bigint");
});