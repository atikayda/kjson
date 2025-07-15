import { assertEquals, assertThrows } from "https://deno.land/std@0.208.0/assert/mod.ts";
import { kJSON, UUID, Decimal128, isUUID, isDecimal128 } from "../mod.ts";

Deno.test("UUID - v4 generation", () => {
  const uuid1 = UUID.v4();
  const uuid2 = UUID.v4();
  
  assertEquals(UUID.isValid(uuid1), true);
  assertEquals(UUID.isValid(uuid2), true);
  assertEquals(uuid1 === uuid2, false); // Should be different
});

Deno.test("UUID - v7 generation", () => {
  const uuid1 = UUID.v7();
  const uuid2 = UUID.v7();
  
  assertEquals(UUID.isValid(uuid1), true);
  assertEquals(UUID.isValid(uuid2), true);
  assertEquals(uuid1 === uuid2, false); // Should be different
  
  // v7 UUIDs should have version 7
  const u = new UUID(uuid1);
  assertEquals(u.version, 7);
});

Deno.test("UUID - parsing and validation", () => {
  const validUuid = "550e8400-e29b-41d4-a716-446655440000";
  const uuid = UUID.parse(validUuid);
  
  assertEquals(uuid.toString(), validUuid);
  assertEquals(uuid.equals(validUuid), true);
  assertEquals(uuid.equals(new UUID(validUuid)), true);
});

Deno.test("UUID - bytes conversion", () => {
  const uuid = new UUID("550e8400-e29b-41d4-a716-446655440000");
  const bytes = uuid.bytes();
  
  assertEquals(bytes.length, 16);
  assertEquals(bytes[0], 0x55);
  assertEquals(bytes[1], 0x0e);
  
  const uuid2 = UUID.fromBytes(bytes);
  assertEquals(uuid.equals(uuid2), true);
});

Deno.test("UUID - kJSON serialization", () => {
  const uuid = new UUID("550e8400-e29b-41d4-a716-446655440000");
  const json = kJSON.stringify(uuid);
  
  // UUID should be serialized as unquoted string
  assertEquals(json, "550e8400-e29b-41d4-a716-446655440000");
  
  // In an object
  const obj = { userId: uuid, name: "Alice" };
  const objJson = kJSON.stringify(obj);
  assertEquals(objJson, '{userId:550e8400-e29b-41d4-a716-446655440000,name:"Alice"}');
});

Deno.test("UUID - kJSON parsing", () => {
  // Direct UUID value
  const uuid1 = kJSON.parse("550e8400-e29b-41d4-a716-446655440000");
  assertEquals(isUUID(uuid1), true);
  assertEquals(uuid1.toString(), "550e8400-e29b-41d4-a716-446655440000");
  
  // UUID in object
  const json = "{id: 550e8400-e29b-41d4-a716-446655440000, name: \"test\"}";
  const parsed = kJSON.parse(json);
  assertEquals(isUUID(parsed.id), true);
  assertEquals(parsed.id.toString(), "550e8400-e29b-41d4-a716-446655440000");
  assertEquals(parsed.name, "test");
});

Deno.test("UUID - quoted UUID remains string", () => {
  const json = '{"id": "550e8400-e29b-41d4-a716-446655440000"}';
  const parsed = kJSON.parse(json);
  
  // Quoted UUID should remain a string, not become a UUID object
  assertEquals(typeof parsed.id, "string");
  assertEquals(parsed.id, "550e8400-e29b-41d4-a716-446655440000");
});

Deno.test("Decimal128 - construction", () => {
  const d1 = new Decimal128("123.45");
  const d2 = new Decimal128(123.45);
  const d3 = new Decimal128(123n);
  
  assertEquals(d1.toString(), "123.45");
  assertEquals(d2.toString(), "123.45");
  assertEquals(d3.toString(), "123");
});

Deno.test("Decimal128 - normalization", () => {
  assertEquals(new Decimal128("123.450").toString(), "123.45");
  assertEquals(new Decimal128("0123.45").toString(), "123.45");
  assertEquals(new Decimal128("-0").toString(), "0");
  assertEquals(new Decimal128("1.23e2").toString(), "123");
});

Deno.test("Decimal128 - validation", () => {
  assertThrows(() => new Decimal128("abc"));
  assertThrows(() => new Decimal128("12.34.56"));
  assertThrows(() => new Decimal128(Infinity));
  assertThrows(() => new Decimal128(NaN));
});

Deno.test("Decimal128 - kJSON serialization", () => {
  const d = new Decimal128("123.45");
  const json = kJSON.stringify(d);
  
  // Decimal128 should be serialized with 'm' suffix
  assertEquals(json, "123.45m");
  
  // In an object
  const obj = { price: d, quantity: 10 };
  const objJson = kJSON.stringify(obj);
  assertEquals(objJson, '{price:123.45m,quantity:10}');
});

Deno.test("Decimal128 - kJSON parsing", () => {
  // Direct decimal value
  const d1 = kJSON.parse("123.45m");
  assertEquals(isDecimal128(d1), true);
  assertEquals(d1.toString(), "123.45");
  
  // Decimal in object
  const json = "{price: 99.99m, quantity: 5}";
  const parsed = kJSON.parse(json);
  assertEquals(isDecimal128(parsed.price), true);
  assertEquals(parsed.price.toString(), "99.99");
  assertEquals(parsed.quantity, 5);
});

Deno.test("Decimal128 - arithmetic operations", () => {
  const d1 = new Decimal128("10.5");
  const d2 = new Decimal128("2.5");
  
  assertEquals(d1.add(d2).toString(), "13");
  assertEquals(d1.subtract(d2).toString(), "8");
  assertEquals(d1.multiply(d2).toString(), "26.25");
  assertEquals(d1.divide(d2).toString(), "4.2");
});

Deno.test("Decimal128 - comparison", () => {
  const d1 = new Decimal128("10.5");
  const d2 = new Decimal128("2.5");
  const d3 = new Decimal128("10.5");
  
  assertEquals(d1.compare(d2), 1);
  assertEquals(d2.compare(d1), -1);
  assertEquals(d1.compare(d3), 0);
  assertEquals(d1.equals(d3), true);
  assertEquals(d1.equals(d2), false);
});

Deno.test("Mixed types in arrays and objects", () => {
  const data = {
    id: new UUID(),
    amount: new Decimal128("99.99"),
    items: [
      { price: new Decimal128("19.99"), id: UUID.v7() },
      { price: new Decimal128("79.99"), id: UUID.v7() }
    ],
    created: new Date(),
    count: 42n
  };
  
  const json = kJSON.stringify(data, { space: 2 });
  const parsed = kJSON.parse(json);
  
  assertEquals(isUUID(parsed.id), true);
  assertEquals(isDecimal128(parsed.amount), true);
  assertEquals(parsed.amount.toString(), "99.99");
  assertEquals(parsed.items.length, 2);
  assertEquals(isDecimal128(parsed.items[0].price), true);
  assertEquals(parsed.items[0].price.toString(), "19.99");
  // Note: nested UUIDs are stored as strings, not UUID objects
  assertEquals(typeof parsed.items[0].id, "string");
  assertEquals(UUID.isValid(parsed.items[0].id), true);
  assertEquals(parsed.created instanceof Date, true);
  assertEquals(typeof parsed.count, "bigint");
});