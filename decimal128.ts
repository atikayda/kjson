// Import the decimal128 polyfill from the proposal-decimal package
import { Decimal as Decimal128Impl } from "npm:proposal-decimal@20250613.0.0";

/**
 * Decimal128 class for IEEE 754-2008 128-bit decimal floating-point numbers
 * Provides 34 decimal digits of precision with exponent range -6143 to +6144
 * Uses the decimal128 polyfill internally for accurate decimal arithmetic
 * Serializes as {number: 123.45m}
 */
export class Decimal128 {
  private readonly impl: Decimal128Impl;

  constructor(value: string | number | bigint | Decimal128Impl) {
    if (value instanceof Decimal128Impl) {
      this.impl = value;
    } else if (typeof value === 'string') {
      // Remove 'm' suffix if present for parsing
      const cleanValue = value.endsWith('m') ? value.slice(0, -1) : value;
      this.impl = new Decimal128Impl(cleanValue);
    } else {
      this.impl = new Decimal128Impl(value);
    }
  }

  /**
   * Check if a string is a valid decimal number
   */
  static isValid(value: string): boolean {
    try {
      // Remove 'm' suffix if present
      const cleanValue = value.endsWith('m') ? value.slice(0, -1) : value;
      new Decimal128Impl(cleanValue);
      return true;
    } catch {
      return false;
    }
  }

  /**
   * Create a Decimal128 from a string
   */
  static fromString(value: string): Decimal128 {
    return new Decimal128(value);
  }

  /**
   * Create a Decimal128 from a number
   */
  static fromNumber(value: number): Decimal128 {
    return new Decimal128(value);
  }

  /**
   * Create a Decimal128 from a BigInt
   */
  static fromBigInt(value: bigint): Decimal128 {
    return new Decimal128(value);
  }

  /**
   * Get zero value
   */
  static get zero(): Decimal128 {
    return new Decimal128(0);
  }

  /**
   * Get one value
   */
  static get one(): Decimal128 {
    return new Decimal128(1);
  }

  /**
   * Parse a decimal with 'm' suffix (e.g., "123.45m")
   */
  static parseWithSuffix(value: string): Decimal128 {
    if (value.endsWith('m')) {
      return new Decimal128(value.slice(0, -1));
    }
    return new Decimal128(value);
  }

  /**
   * Get the string representation
   */
  toString(): string {
    return this.impl.toString();
  }

  /**
   * Get the string representation with 'm' suffix
   */
  toStringWithSuffix(): string {
    return this.impl.toString() + 'm';
  }

  /**
   * Get the numeric value (may lose precision for large numbers)
   */
  toNumber(): number {
    return this.impl.toNumber();
  }

  /**
   * Convert to BigInt (truncates decimal part)
   */
  toBigInt(): bigint {
    // The polyfill throws if not an integer, so we truncate first
    const truncated = this.impl.round(0);
    return truncated.toBigInt();
  }

  /**
   * Check if this decimal equals another
   */
  equals(other: Decimal128 | string | number): boolean {
    const otherDecimal = other instanceof Decimal128 ? other : new Decimal128(other);
    return this.impl.equals(otherDecimal.impl);
  }

  /**
   * Check if this decimal is zero
   */
  isZero(): boolean {
    return this.impl.equals(new Decimal128Impl("0"));
  }

  /**
   * Check if this decimal is negative
   */
  isNegative(): boolean {
    return this.impl.isNegative();
  }

  /**
   * Check if this decimal is positive (and not zero)
   */
  isPositive(): boolean {
    return !this.impl.isNegative() && !this.impl.equals(new Decimal128Impl("0"));
  }

  /**
   * Create a new Decimal128 instance with the same value
   */
  clone(): Decimal128 {
    // Create new instance from the implementation
    return new Decimal128(this.impl);
  }

  /**
   * Compare with another decimal
   * Returns: -1 if this < other, 0 if equal, 1 if this > other
   */
  compare(other: Decimal128 | string | number): number {
    const otherDecimal = other instanceof Decimal128 ? other : new Decimal128(other);
    if (this.impl.equals(otherDecimal.impl)) return 0;
    return this.impl.lessThan(otherDecimal.impl) ? -1 : 1;
  }

  /**
   * Add another decimal to this one
   * Uses IEEE 754-2008 decimal arithmetic for accurate results
   */
  add(other: Decimal128 | string | number): Decimal128 {
    const otherDecimal = other instanceof Decimal128 ? other : new Decimal128(other);
    return new Decimal128(this.impl.add(otherDecimal.impl));
  }

  /**
   * Subtract another decimal from this one
   */
  subtract(other: Decimal128 | string | number): Decimal128 {
    const otherDecimal = other instanceof Decimal128 ? other : new Decimal128(other);
    return new Decimal128(this.impl.subtract(otherDecimal.impl));
  }

  /**
   * Multiply by another decimal
   */
  multiply(other: Decimal128 | string | number): Decimal128 {
    const otherDecimal = other instanceof Decimal128 ? other : new Decimal128(other);
    return new Decimal128(this.impl.multiply(otherDecimal.impl));
  }

  /**
   * Divide by another decimal
   */
  divide(other: Decimal128 | string | number): Decimal128 {
    const otherDecimal = other instanceof Decimal128 ? other : new Decimal128(other);
    return new Decimal128(this.impl.divide(otherDecimal.impl));
  }

  /**
   * Get absolute value
   */
  abs(): Decimal128 {
    return new Decimal128(this.impl.abs());
  }

  /**
   * Negate the value
   */
  negate(): Decimal128 {
    return new Decimal128(this.impl.negate());
  }

  /**
   * Get the remainder after division
   */
  remainder(other: Decimal128 | string | number): Decimal128 {
    const otherDecimal = other instanceof Decimal128 ? other : new Decimal128(other);
    return new Decimal128(this.impl.remainder(otherDecimal.impl));
  }

  /**
   * Round to a given number of decimal places
   */
  round(options?: { decimalPlaces?: number; roundingMode?: "ceil" | "floor" | "trunc" | "halfEven" | "halfExpand" }): Decimal128 {
    const decimalPlaces = options?.decimalPlaces || 0;
    const rounded = this.impl.round(decimalPlaces);
    return new Decimal128(rounded);
  }

  /**
   * Check if this decimal is NaN
   */
  isNaN(): boolean {
    return this.impl.isNaN();
  }

  /**
   * Check if this decimal is finite
   */
  isFinite(): boolean {
    return this.impl.isFinite();
  }

  /**
   * Get string in fixed-point notation
   */
  toFixed(fractionDigits?: number): string {
    return this.impl.toFixed(fractionDigits ? { digits: fractionDigits } : undefined);
  }

  /**
   * Get string in precision notation
   */
  toPrecision(precision?: number): string {
    return this.impl.toPrecision(precision ? { digits: precision } : undefined);
  }
}

// Type guard
export function isDecimal128(value: unknown): value is Decimal128 {
  return value instanceof Decimal128;
}