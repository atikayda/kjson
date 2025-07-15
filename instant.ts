/**
 * Instant and Duration types with nanosecond precision in Zulu time (UTC)
 * 
 * This uses Temporal.Instant and Temporal.Duration when available,
 * falling back to polyfill implementations only when Temporal is unavailable.
 */

// Check if Temporal is available
const TEMPORAL_AVAILABLE = typeof globalThis.Temporal !== 'undefined' && 
                          typeof globalThis.Temporal.Instant !== 'undefined' &&
                          typeof globalThis.Temporal.Duration !== 'undefined';

/**
 * Use Temporal.Instant if available, otherwise use our polyfill
 */
export const Instant = TEMPORAL_AVAILABLE ? globalThis.Temporal.Instant : class InstantPolyfill {
  private readonly nanos: bigint;

  constructor(nanos: bigint) {
    this.nanos = nanos;
  }

  get epochNanoseconds(): bigint {
    return this.nanos;
  }

  get epochMilliseconds(): number {
    return Number(this.nanos / 1_000_000n);
  }

  get epochSeconds(): number {
    return Number(this.nanos / 1_000_000_000n);
  }

  static fromEpochNanoseconds(nanos: bigint): InstantPolyfill {
    return new InstantPolyfill(nanos);
  }

  static fromEpochMilliseconds(millis: number): InstantPolyfill {
    return new InstantPolyfill(BigInt(millis) * 1_000_000n);
  }

  static fromEpochSeconds(seconds: number): InstantPolyfill {
    return new InstantPolyfill(BigInt(Math.floor(seconds)) * 1_000_000_000n);
  }

  static now(): InstantPolyfill {
    const millis = Date.now();
    return new InstantPolyfill(BigInt(millis) * 1_000_000n);
  }

  static from(isoString: string): InstantPolyfill {
    // Convert to Zulu time if it has a timezone
    let zuluString = isoString;
    
    if (isoString.includes('+') || (isoString.includes('-') && isoString.lastIndexOf('-') > 10)) {
      // Has timezone offset, convert to Zulu
      const date = new Date(isoString);
      if (isNaN(date.getTime())) {
        throw new RangeError(`Invalid ISO date string: ${isoString}`);
      }
      zuluString = date.toISOString();
    } else if (!isoString.endsWith('Z')) {
      // No timezone specified, assume Zulu
      zuluString = isoString.endsWith('Z') ? isoString : isoString + 'Z';
    }

    // Parse the Zulu string manually to preserve nanosecond precision
    const match = zuluString.match(/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.(\d+))?Z$/);
    if (!match) {
      throw new RangeError(`Invalid ISO date string format: ${isoString}`);
    }

    const [, year, month, day, hour, minute, second, fractionStr] = match;
    
    // Create Date object for the main parts
    const date = new Date(Date.UTC(
      parseInt(year, 10),
      parseInt(month, 10) - 1, // Month is 0-based
      parseInt(day, 10),
      parseInt(hour, 10),
      parseInt(minute, 10),
      parseInt(second, 10)
    ));

    if (isNaN(date.getTime())) {
      throw new RangeError(`Invalid date components: ${isoString}`);
    }

    let nanos = BigInt(date.getTime()) * 1_000_000n;

    // Handle fractional seconds
    if (fractionStr) {
      // Pad or truncate to 9 digits (nanoseconds)
      const paddedFraction = fractionStr.padEnd(9, '0').slice(0, 9);
      const fractionNanos = BigInt(paddedFraction);
      nanos += fractionNanos;
    }

    return new InstantPolyfill(nanos);
  }

  toString(): string {
    const millis = this.epochMilliseconds;
    const date = new Date(millis);
    
    // Get the base ISO string
    const baseIso = date.toISOString();
    
    // Calculate nanoseconds within the millisecond
    const remainingNanos = this.nanos - BigInt(millis) * 1_000_000n;
    
    if (remainingNanos === 0n) {
      // No sub-millisecond precision, return standard format
      return baseIso;
    }

    // Replace the millisecond part with full nanosecond precision
    const dotIndex = baseIso.indexOf('.');
    const beforeDot = baseIso.slice(0, dotIndex);
    
    // Format nanoseconds (pad to 9 digits, then remove trailing zeros)
    const totalFractionalNanos = Number(this.nanos % 1_000_000_000n);
    const fractionalStr = totalFractionalNanos.toString().padStart(9, '0');
    const trimmedFractional = fractionalStr.replace(/0+$/, '');
    
    if (trimmedFractional === '') {
      return beforeDot + 'Z';
    }
    
    return beforeDot + '.' + trimmedFractional + 'Z';
  }

  toJSON(): string {
    return this.toString();
  }

  valueOf(): number {
    return this.epochMilliseconds;
  }

  toDate(): Date {
    return new Date(this.epochMilliseconds);
  }

  compare(other: InstantPolyfill): number {
    if (this.nanos < other.nanos) return -1;
    if (this.nanos > other.nanos) return 1;
    return 0;
  }

  equals(other: InstantPolyfill): boolean {
    return this.nanos === other.nanos;
  }

  add(duration: any): InstantPolyfill {
    const durationNanos = isDuration(duration) ? duration.total('nanoseconds') : duration.toNanos();
    return new InstantPolyfill(this.nanos + BigInt(Math.floor(durationNanos)));
  }

  subtract(duration: any): InstantPolyfill {
    const durationNanos = isDuration(duration) ? duration.total('nanoseconds') : duration.toNanos();
    return new InstantPolyfill(this.nanos - BigInt(Math.floor(durationNanos)));
  }

  since(other: InstantPolyfill): any {
    const nanosDiff = this.nanos - other.nanos;
    return Duration.from({ nanoseconds: Number(nanosDiff) });
  }

  until(other: InstantPolyfill): any {
    const nanosDiff = other.nanos - this.nanos;
    return Duration.from({ nanoseconds: Number(nanosDiff) });
  }
};

/**
 * Use Temporal.Duration if available, otherwise use our polyfill
 */
export const Duration = TEMPORAL_AVAILABLE ? globalThis.Temporal.Duration : class DurationPolyfill {
  private readonly nanos: bigint;

  constructor(nanos: bigint) {
    this.nanos = nanos;
  }

  toNanos(): bigint {
    return this.nanos;
  }

  total(unit: string): number {
    switch (unit) {
      case 'nanoseconds':
        return Number(this.nanos);
      case 'microseconds':
        return Number(this.nanos / 1_000n);
      case 'milliseconds':
        return Number(this.nanos / 1_000_000n);
      case 'seconds':
        return Number(this.nanos / 1_000_000_000n);
      case 'minutes':
        return Number(this.nanos / 60_000_000_000n);
      case 'hours':
        return Number(this.nanos / 3_600_000_000_000n);
      case 'days':
        return Number(this.nanos / 86_400_000_000_000n);
      default:
        throw new RangeError(`Unknown unit: ${unit}`);
    }
  }

  static from(input: string | object): DurationPolyfill {
    if (typeof input === 'string') {
      return DurationPolyfill.parse(input);
    }
    
    if (typeof input === 'object' && input !== null) {
      const obj = input as any;
      let totalNanos = 0n;
      
      if (obj.days) totalNanos += BigInt(obj.days) * 86_400_000_000_000n;
      if (obj.hours) totalNanos += BigInt(obj.hours) * 3_600_000_000_000n;
      if (obj.minutes) totalNanos += BigInt(obj.minutes) * 60_000_000_000n;
      if (obj.seconds) totalNanos += BigInt(Math.floor(obj.seconds)) * 1_000_000_000n;
      if (obj.milliseconds) totalNanos += BigInt(obj.milliseconds) * 1_000_000n;
      if (obj.microseconds) totalNanos += BigInt(obj.microseconds) * 1_000n;
      if (obj.nanoseconds) totalNanos += BigInt(obj.nanoseconds);
      
      return new DurationPolyfill(totalNanos);
    }
    
    throw new TypeError('Invalid Duration input');
  }

  static parse(durationString: string): DurationPolyfill {
    const match = durationString.match(/^P(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?(?:(\d+(?:\.\d+)?)S)?)?$/);
    if (!match) {
      throw new RangeError(`Invalid ISO duration format: ${durationString}`);
    }

    const [, days, hours, minutes, seconds] = match;
    
    let totalNanos = 0n;
    
    if (days) {
      totalNanos += BigInt(days) * 86_400_000_000_000n;
    }
    if (hours) {
      totalNanos += BigInt(hours) * 3_600_000_000_000n;
    }
    if (minutes) {
      totalNanos += BigInt(minutes) * 60_000_000_000n;
    }
    if (seconds) {
      const sec = parseFloat(seconds);
      totalNanos += BigInt(Math.floor(sec * 1_000_000_000));
    }

    return new DurationPolyfill(totalNanos);
  }

  toString(): string {
    if (this.nanos === 0n) {
      return 'PT0S';
    }

    let remaining = this.nanos;
    let result = 'P';
    
    // Days
    const days = remaining / 86_400_000_000_000n;
    if (days > 0n) {
      result += days.toString() + 'D';
      remaining = remaining % 86_400_000_000_000n;
    }

    if (remaining > 0n) {
      result += 'T';
      
      // Hours
      const hours = remaining / 3_600_000_000_000n;
      if (hours > 0n) {
        result += hours.toString() + 'H';
        remaining = remaining % 3_600_000_000_000n;
      }

      // Minutes
      const minutes = remaining / 60_000_000_000n;
      if (minutes > 0n) {
        result += minutes.toString() + 'M';
        remaining = remaining % 60_000_000_000n;
      }

      // Seconds (with fractional part)
      if (remaining > 0n) {
        const seconds = remaining / 1_000_000_000n;
        const nanosPart = remaining % 1_000_000_000n;
        
        if (nanosPart === 0n) {
          result += seconds.toString() + 'S';
        } else {
          const fractionalStr = nanosPart.toString().padStart(9, '0').replace(/0+$/, '');
          result += seconds.toString() + '.' + fractionalStr + 'S';
        }
      }
    }

    return result;
  }

  toJSON(): string {
    return this.toString();
  }

  add(other: DurationPolyfill): DurationPolyfill {
    return new DurationPolyfill(this.nanos + other.nanos);
  }

  subtract(other: DurationPolyfill): DurationPolyfill {
    return new DurationPolyfill(this.nanos - other.nanos);
  }

  negated(): DurationPolyfill {
    return new DurationPolyfill(-this.nanos);
  }

  abs(): DurationPolyfill {
    return new DurationPolyfill(this.nanos < 0n ? -this.nanos : this.nanos);
  }

  compare(other: DurationPolyfill): number {
    if (this.nanos < other.nanos) return -1;
    if (this.nanos > other.nanos) return 1;
    return 0;
  }

  equals(other: DurationPolyfill): boolean {
    return this.nanos === other.nanos;
  }

  get sign(): number {
    if (this.nanos > 0n) return 1;
    if (this.nanos < 0n) return -1;
    return 0;
  }
};

/**
 * Type guard to check if a value is an Instant
 */
export function isInstant(value: any): value is typeof Instant {
  if (TEMPORAL_AVAILABLE) {
    return value instanceof globalThis.Temporal.Instant;
  }
  return value instanceof (Instant as any);
}

/**
 * Type guard to check if a value is a Duration
 */
export function isDuration(value: any): value is typeof Duration {
  if (TEMPORAL_AVAILABLE) {
    return value instanceof globalThis.Temporal.Duration;
  }
  return value instanceof (Duration as any);
}