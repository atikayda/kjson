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
  const jsonStr = `{single: 'value', double: "value2", backtick: \`template\`}`;
  const parsed = parse(jsonStr);
  
  assertEquals(parsed.single, "value");
  assertEquals(parsed.double, "value2");
  assertEquals(parsed.backtick, "template");
});

Deno.test("kJSON - Backtick strings with quotes inside", () => {
  const input = `{text: \`He said "Hello" and she replied 'Hi'\`}`;
  const result = parse(input);
  
  assertEquals(result.text, `He said "Hello" and she replied 'Hi'`);
});

Deno.test("kJSON - Multiline backtick strings", () => {
  const input = `{
    text: \`Line 1
Line 2
Line 3\`
  }`;
  const result = parse(input);
  
  assertEquals(result.text, "Line 1\nLine 2\nLine 3");
});

Deno.test("kJSON - Escaped backticks", () => {
  const input = `{text: \`This has a \\\` backtick\`}`;
  const result = parse(input);
  
  assertEquals(result.text, "This has a ` backtick");
});

Deno.test("kJSON - Smart quote selection in stringify", () => {
  // No quotes - should use single quotes (default)
  assertEquals(stringify({text: "hello"}), `{text: 'hello'}`);
  
  // Has single quotes - should use double quotes
  assertEquals(stringify({text: "it's nice"}), `{text: "it's nice"}`);
  
  // Has double quotes - should use single quotes
  assertEquals(stringify({text: 'He said "hi"'}), `{text: 'He said "hi"'}`);
  
  // Has both single and double - should use backticks
  assertEquals(stringify({text: `He said "hello" and 'hi'`}), `{text: \`He said "hello" and 'hi'\`}`);
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
  
  // Test with Temporal.Instant if available
  if (typeof Temporal !== 'undefined' && Temporal.Instant) {
    assertEquals(kJSON.isValid('{ instant: 2025-01-01T00:00:00.000Z }', { parseTemporalInstants: true }), true);
  }
});

Deno.test("kJSON - kJSON.safeParseWith", () => {
  const result1 = kJSON.safeParseWith('{ name: "test" }', { default: true });
  assertEquals((result1 as any).name, "test");
  
  const result2 = kJSON.safeParseWith('{ invalid }', { default: true });
  assertEquals((result2 as any).default, true);
  
  // Test with Temporal.Instant if available
  if (typeof Temporal !== 'undefined' && Temporal.Instant) {
    const result3 = kJSON.safeParseWith('{ instant: 2025-01-01T00:00:00.000Z }', { default: true }, { parseTemporalInstants: true });
    assertEquals((result3 as any).instant instanceof Temporal.Instant, true);
  }
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
  
  // Test with Temporal.Instant if available
  if (typeof Temporal !== 'undefined' && Temporal.Instant) {
    const temporalParser = kJSON.createParser({
      parseTemporalInstants: true,
      parseDates: false
    });
    
    const result2 = temporalParser('{ instant: 2025-01-01T00:00:00.000Z }');
    assertEquals(result2.instant instanceof Temporal.Instant, true);
  }
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
  
  // Test with Temporal.Instant if available
  if (typeof Temporal !== 'undefined' && Temporal.Instant) {
    const temporalStringifier = kJSON.createStringifier({
      serializeTemporalInstants: true,
      space: 2
    });
    
    const instant = Temporal.Instant.from("2025-01-01T00:00:00.000Z");
    const json2 = temporalStringifier({ instant: instant });
    const parsed2 = parse(json2, { parseTemporalInstants: true });
    
    assertEquals(parsed2.instant instanceof Temporal.Instant, true);
    assertEquals(parsed2.instant.equals(instant), true);
  }
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

// Temporal.Instant tests (requires --unstable-temporal)
Deno.test("kJSON - Temporal.Instant support", () => {
  // Skip test if Temporal is not available
  if (typeof Temporal === 'undefined' || !Temporal.Instant) {
    console.log("Skipping Temporal.Instant test - Temporal API not available");
    return;
  }

  const instant = Temporal.Instant.from("2025-01-01T00:00:00.000Z");
  const input = {
    created: instant,
    updated: Temporal.Instant.from("2025-12-31T23:59:59.999Z")
  };
  
  const json = stringify(input, { serializeTemporalInstants: true });
  const parsed = parse(json, { parseTemporalInstants: true });
  
  assertEquals(parsed.created instanceof Temporal.Instant, true);
  assertEquals(parsed.created.equals(instant), true);
  assertEquals(parsed.updated instanceof Temporal.Instant, true);
});

Deno.test("kJSON - Temporal.Instant vs Date distinction", () => {
  // Skip test if Temporal is not available
  if (typeof Temporal === 'undefined' || !Temporal.Instant) {
    console.log("Skipping Temporal.Instant test - Temporal API not available");
    return;
  }

  const instant = Temporal.Instant.from("2025-01-01T00:00:00.000Z");
  const date = new Date("2025-01-01T00:00:00.000Z");
  const input = {
    instant: instant,
    date: date,
    instantString: "2025-01-01T00:00:00.000Z",
    description: "This is a timestamp: 2025-01-01T00:00:00.000Z"
  };
  
  // Test separate parsing modes for round-trip consistency
  const jsonTemporal = stringify(input, { serializeTemporalInstants: true, serializeDates: false });
  const parsedTemporal = parse(jsonTemporal, { parseTemporalInstants: true, parseDates: false });
  
  assertEquals(parsedTemporal.instant instanceof Temporal.Instant, true);
  assertEquals(typeof parsedTemporal.date, "string"); // Date serialized as quoted string
  assertEquals(typeof parsedTemporal.instantString, "string");
  assertEquals(typeof parsedTemporal.description, "string");
  
  const jsonDate = stringify(input, { serializeTemporalInstants: false, serializeDates: true });
  const parsedDate = parse(jsonDate, { parseTemporalInstants: false, parseDates: true });
  
  assertEquals(typeof parsedDate.instant, "string"); // Temporal.Instant serialized as quoted string
  assertEquals(parsedDate.date instanceof Date, true);
  assertEquals(typeof parsedDate.instantString, "string");
  assertEquals(typeof parsedDate.description, "string");
});

Deno.test("kJSON - Temporal.Instant preference over Date", () => {
  // Skip test if Temporal is not available
  if (typeof Temporal === 'undefined' || !Temporal.Instant) {
    console.log("Skipping Temporal.Instant test - Temporal API not available");
    return;
  }

  const isoString = "2025-01-01T00:00:00.000Z";
  const jsonStr = `{ timestamp: ${isoString} }`;
  
  // When both parseTemporalInstants and parseDates are true, Temporal.Instant should take precedence
  const parsed = parse(jsonStr, { parseTemporalInstants: true, parseDates: true });
  
  assertEquals(parsed.timestamp instanceof Temporal.Instant, true);
  assertEquals(parsed.timestamp instanceof Date, false);
});

Deno.test("kJSON - Temporal.Instant various formats", () => {
  // Skip test if Temporal is not available
  if (typeof Temporal === 'undefined' || !Temporal.Instant) {
    console.log("Skipping Temporal.Instant test - Temporal API not available");
    return;
  }

  const formats = [
    "2025-01-01T00:00:00.000Z",
    "2025-01-01T00:00:00Z",
    "2025-01-01T00:00:00.123456789Z",
    "2025-01-01T00:00:00+00:00",
    "2025-01-01T12:30:45.123+05:30"
  ];
  
  formats.forEach(format => {
    const input = { timestamp: Temporal.Instant.from(format) };
    const json = stringify(input, { serializeTemporalInstants: true });
    const parsed = parse(json, { parseTemporalInstants: true });
    
    assertEquals(parsed.timestamp instanceof Temporal.Instant, true);
    assertEquals(parsed.timestamp.equals(input.timestamp), true);
  });
});

Deno.test("kJSON - Temporal.Instant in arrays", () => {
  // Skip test if Temporal is not available
  if (typeof Temporal === 'undefined' || !Temporal.Instant) {
    console.log("Skipping Temporal.Instant test - Temporal API not available");
    return;
  }

  const instants = [
    Temporal.Instant.from("2025-01-01T00:00:00.000Z"),
    Temporal.Instant.from("2025-02-01T00:00:00.000Z"),
    Temporal.Instant.from("2025-03-01T00:00:00.000Z")
  ];
  
  const json = stringify(instants, { serializeTemporalInstants: true });
  const parsed = parse(json, { parseTemporalInstants: true });
  
  assertEquals(parsed.length, 3);
  assertEquals(parsed[0] instanceof Temporal.Instant, true);
  assertEquals(parsed[0].equals(instants[0]), true);
  assertEquals(parsed[1].equals(instants[1]), true);
  assertEquals(parsed[2].equals(instants[2]), true);
});

Deno.test("kJSON - Temporal.Instant serialization options", () => {
  // Skip test if Temporal is not available
  if (typeof Temporal === 'undefined' || !Temporal.Instant) {
    console.log("Skipping Temporal.Instant test - Temporal API not available");
    return;
  }

  const instant = Temporal.Instant.from("2025-01-01T00:00:00.000Z");
  const input = { timestamp: instant };
  
  // With serializeTemporalInstants: true (default for new option)
  const json1 = stringify(input, { serializeTemporalInstants: true });
  const parsed1 = parse(json1, { parseTemporalInstants: true });
  assertEquals(parsed1.timestamp instanceof Temporal.Instant, true);
  
  // With serializeTemporalInstants: false (should still work but as quoted string)
  const json2 = stringify(input, { serializeTemporalInstants: false });
  const parsed2 = parse(json2, { parseTemporalInstants: true });
  assertEquals(typeof parsed2.timestamp, "string");
  assertEquals(parsed2.timestamp, instant.toJSON());
});

Deno.test("kJSON - Mixed BigInt, Date, and Temporal.Instant", () => {
  // Skip test if Temporal is not available
  if (typeof Temporal === 'undefined' || !Temporal.Instant) {
    console.log("Skipping Temporal.Instant test - Temporal API not available");
    return;
  }

  const input = {
    id: 12345n,
    created: new Date("2025-01-01T00:00:00.000Z"),
    updated: Temporal.Instant.from("2025-01-01T12:00:00.000Z"),
    count: 42,
    active: true
  };
  
  // Test with Temporal.Instant parsing mode (both ISO strings will become Temporal.Instant)
  const json = stringify(input, { 
    bigintSuffix: true,
    serializeDates: true,
    serializeTemporalInstants: true
  });
  const parsed = parse(json, { 
    parseDates: false,
    parseTemporalInstants: true
  });
  
  assertEquals(typeof parsed.id, "bigint");
  assertEquals(parsed.created instanceof Temporal.Instant, true); // Date becomes Temporal.Instant
  assertEquals(parsed.updated instanceof Temporal.Instant, true);
  assertEquals(typeof parsed.count, "number");
  assertEquals(typeof parsed.active, "boolean");
});

Deno.test("kJSON - Temporal.Instant disabled by default", () => {
  // Skip test if Temporal is not available
  if (typeof Temporal === 'undefined' || !Temporal.Instant) {
    console.log("Skipping Temporal.Instant test - Temporal API not available");
    return;
  }

  const isoString = "2025-01-01T00:00:00.000Z";
  const jsonStr = `{ timestamp: ${isoString} }`;
  
  // Default parsing should not create Temporal.Instant
  const parsed = parse(jsonStr);
  assertEquals(parsed.timestamp instanceof Date, true);
  assertEquals(parsed.timestamp instanceof Temporal.Instant, false);
});

Deno.test("kJSON - Complex nested with Temporal.Instant", () => {
  // Skip test if Temporal is not available
  if (typeof Temporal === 'undefined' || !Temporal.Instant) {
    console.log("Skipping Temporal.Instant test - Temporal API not available");
    return;
  }

  const input = {
    metadata: {
      id: 12345n,
      created: Temporal.Instant.from("2025-01-01T00:00:00.000Z"),
      version: "1.0"
    },
    events: [
      { 
        timestamp: Temporal.Instant.from("2025-01-01T10:00:00.000Z"), 
        type: "start",
        value: 100n 
      },
      { 
        timestamp: Temporal.Instant.from("2025-01-01T11:00:00.000Z"), 
        type: "update",
        value: 200n 
      }
    ],
    config: {
      enabled: true,
      maxRetries: 3
    }
  };
  
  const json = stringify(input, { 
    bigintSuffix: true,
    serializeTemporalInstants: true 
  });
  const parsed = parse(json, { 
    parseTemporalInstants: true 
  });
  
  assertEquals(typeof parsed.metadata.id, "bigint");
  assertEquals(parsed.metadata.created instanceof Temporal.Instant, true);
  assertEquals(parsed.events[0].timestamp instanceof Temporal.Instant, true);
  assertEquals(typeof parsed.events[0].value, "bigint");
  assertEquals(parsed.config.enabled, true);
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