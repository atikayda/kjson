import { assertEquals, assertThrows } from "https://deno.land/std@0.208.0/assert/mod.ts";
import { parse, stringify } from "../mod.ts";
import { Decimal128 } from "../decimal128.ts";

Deno.test("Decimal128 - Basic construction and serialization", () => {
  const d1 = new Decimal128("123.45");
  assertEquals(d1.toString(), "123.45");
  assertEquals(d1.toStringWithSuffix(), "123.45m");
  
  const d2 = new Decimal128("99.99m"); // with suffix
  assertEquals(d2.toString(), "99.99");
  
  const d3 = new Decimal128(456.78);
  assertEquals(d3.toString(), "456.78");
  
  const d4 = new Decimal128(123n);
  assertEquals(d4.toString(), "123");
});

Deno.test("Decimal128 - Accurate decimal arithmetic", () => {
  // The classic problem with binary floating point
  const a = new Decimal128("0.1");
  const b = new Decimal128("0.2");
  const sum = a.add(b);
  
  // This should be exactly 0.3 with decimal arithmetic
  assertEquals(sum.toString(), "0.3");
  assertEquals(sum.equals("0.3"), true);
  
  // More precision tests
  const x = new Decimal128("99.99");
  const y = new Decimal128("0.01");
  const total = x.add(y);
  assertEquals(total.toString(), "100");
  
  // Multiplication
  const price = new Decimal128("19.99");
  const qty = new Decimal128("3");
  const subtotal = price.multiply(qty);
  assertEquals(subtotal.toString(), "59.97");
});

Deno.test("Decimal128 - kJSON serialization and parsing", () => {
  const data = {
    price: new Decimal128("99.99"),
    tax: new Decimal128("8.25"),
    total: new Decimal128("108.24")
  };
  
  const json = stringify(data);
  assertEquals(json, '{price: 99.99m, tax: 8.25m, total: 108.24m}');
  
  const parsed = parse(json);
  assertEquals(parsed.price instanceof Decimal128, true);
  assertEquals(parsed.price.toString(), "99.99");
  assertEquals(parsed.tax.toString(), "8.25");
  assertEquals(parsed.total.toString(), "108.24");
});

Deno.test("Decimal128 - Edge cases", () => {
  // Zero
  const zero = Decimal128.zero;
  assertEquals(zero.toString(), "0");
  assertEquals(zero.isZero(), true);
  
  // Negative
  const neg = new Decimal128("-123.45");
  assertEquals(neg.toString(), "-123.45");
  assertEquals(neg.isNegative(), true);
  assertEquals(neg.abs().toString(), "123.45");
  
  // Large numbers
  const large = new Decimal128("999999999999999999999999999999.99");
  assertEquals(large.toString(), "999999999999999999999999999999.99");
});

Deno.test("Decimal128 - Comparison operations", () => {
  const a = new Decimal128("100");
  const b = new Decimal128("99.99");
  const c = new Decimal128("100.00");
  
  assertEquals(a.equals(c), true);
  assertEquals(a.equals(b), false);
  assertEquals(a.compare(b) > 0, true);
  assertEquals(b.compare(a) < 0, true);
  assertEquals(a.compare(c), 0);
});

Deno.test("Decimal128 - Division and remainder", () => {
  const a = new Decimal128("100");
  const b = new Decimal128("3");
  
  const quotient = a.divide(b);
  // Should get precise decimal result
  assertEquals(quotient.toString().startsWith("33.333333"), true);
  
  const remainder = a.remainder(b);
  assertEquals(remainder.toString(), "1");
});

Deno.test("Decimal128 - Rounding operations", () => {
  const pi = new Decimal128("3.14159265358979323846");
  
  const rounded2 = pi.round({ decimalPlaces: 2 });
  assertEquals(rounded2.toString(), "3.14");
  
  const rounded4 = pi.round({ decimalPlaces: 4 });
  assertEquals(rounded4.toString(), "3.1416");
  
  // Test different rounding modes
  const val = new Decimal128("2.5");
  const up = val.round({ decimalPlaces: 0, roundingMode: "ceil" });
  assertEquals(up.toString(), "3");
  
  const down = val.round({ decimalPlaces: 0, roundingMode: "floor" });
  assertEquals(down.toString(), "2");
});

Deno.test("Decimal128 - Invalid input handling", () => {
  assertThrows(() => new Decimal128("not a number"));
  assertThrows(() => new Decimal128("123.45.67"));
  
  // Test isValid static method
  assertEquals(Decimal128.isValid("123.45"), true);
  assertEquals(Decimal128.isValid("99.99m"), true);
  assertEquals(Decimal128.isValid("invalid"), false);
});

Deno.test("Decimal128 - Complex kJSON example", () => {
  const invoice = {
    id: "INV-001",
    items: [
      { name: "Widget", price: new Decimal128("19.99"), qty: 3 },
      { name: "Gadget", price: new Decimal128("49.95"), qty: 2 }
    ],
    subtotal: new Decimal128("159.87"),
    tax: new Decimal128("12.79"),
    total: new Decimal128("172.66")
  };
  
  const json = stringify(invoice, { space: 2 });
  const parsed = parse(json);
  
  // Verify all decimals are preserved
  assertEquals(parsed.items[0].price instanceof Decimal128, true);
  assertEquals(parsed.total.toString(), "172.66");
  
  // Verify arithmetic
  const calculatedSubtotal = parsed.items[0].price.multiply(3)
    .add(parsed.items[1].price.multiply(2));
  assertEquals(calculatedSubtotal.toString(), "159.87");
});