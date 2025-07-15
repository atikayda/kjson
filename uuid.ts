/**
 * UUID class supporting v4 and v7 generation and parsing
 * Serializes as {id: 01912d68-783e-7a03-8467-5661c1243ad4} (unquoted)
 */
export class UUID {
  private readonly value: string;

  constructor(uuid?: string) {
    if (uuid) {
      if (!UUID.isValid(uuid)) {
        throw new Error(`Invalid UUID format: ${uuid}`);
      }
      this.value = uuid.toLowerCase();
    } else {
      // Default to v4 generation
      this.value = UUID.v4();
    }
  }

  /**
   * Generate a UUIDv4 (random)
   */
  static v4(): string {
    const bytes = crypto.getRandomValues(new Uint8Array(16));
    
    // Set version (4) and variant bits
    bytes[6] = (bytes[6] & 0x0f) | 0x40; // Version 4
    bytes[8] = (bytes[8] & 0x3f) | 0x80; // Variant 10
    
    return UUID.bytesToString(bytes);
  }

  /**
   * Generate a UUIDv7 (timestamp-based)
   */
  static v7(): string {
    const bytes = new Uint8Array(16);
    const now = Date.now();
    
    // Timestamp (48 bits)
    bytes[0] = (now >>> 40) & 0xff;
    bytes[1] = (now >>> 32) & 0xff;
    bytes[2] = (now >>> 24) & 0xff;
    bytes[3] = (now >>> 16) & 0xff;
    bytes[4] = (now >>> 8) & 0xff;
    bytes[5] = now & 0xff;
    
    // Random data for the rest
    const random = crypto.getRandomValues(new Uint8Array(10));
    for (let i = 6; i < 16; i++) {
      bytes[i] = random[i - 6];
    }
    
    // Set version (7) and variant bits
    bytes[6] = (bytes[6] & 0x0f) | 0x70; // Version 7
    bytes[8] = (bytes[8] & 0x3f) | 0x80; // Variant 10
    
    return UUID.bytesToString(bytes);
  }

  /**
   * Parse a UUID string
   */
  static parse(uuid: string): UUID {
    return new UUID(uuid);
  }

  /**
   * Check if a string is a valid UUID
   */
  static isValid(uuid: string): boolean {
    const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
    return uuidRegex.test(uuid);
  }

  /**
   * Convert bytes to UUID string format
   */
  private static bytesToString(bytes: Uint8Array): string {
    const hex = Array.from(bytes)
      .map(b => b.toString(16).padStart(2, '0'))
      .join('');
    
    return [
      hex.slice(0, 8),
      hex.slice(8, 12),
      hex.slice(12, 16),
      hex.slice(16, 20),
      hex.slice(20, 32)
    ].join('-');
  }

  /**
   * Get the UUID string value
   */
  toString(): string {
    return this.value;
  }

  /**
   * Get the UUID version
   */
  get version(): number {
    const versionChar = this.value[14];
    return parseInt(versionChar, 16);
  }

  /**
   * Compare two UUIDs for equality
   */
  equals(other: UUID | string): boolean {
    const otherValue = other instanceof UUID ? other.value : other.toLowerCase();
    return this.value === otherValue;
  }

  /**
   * Create a new UUID instance with the same value
   */
  clone(): UUID {
    return new UUID(this.value);
  }

  /**
   * Convert UUID string to bytes
   */
  bytes(): Uint8Array {
    const hex = this.value.replace(/-/g, '');
    const bytes = new Uint8Array(16);
    
    for (let i = 0; i < 16; i++) {
      bytes[i] = parseInt(hex.substr(i * 2, 2), 16);
    }
    
    return bytes;
  }

  /**
   * Create UUID from bytes
   */
  static fromBytes(bytes: Uint8Array): UUID {
    if (bytes.length !== 16) {
      throw new Error('UUID must be exactly 16 bytes');
    }
    return new UUID(UUID.bytesToString(bytes));
  }
}

// Type guard
export function isUUID(value: unknown): value is UUID {
  return value instanceof UUID;
}