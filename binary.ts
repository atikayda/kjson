/**
 * kJSONB - Binary format for kJSON
 * 
 * A compact, self-describing binary format that preserves all kJSON types
 * including BigInt, UUID, Decimal128, Date, and more.
 * 
 * Format Features:
 * - Type-first encoding with single byte type tags
 * - Variable-length integer encoding (varint) for efficiency
 * - IEEE 754 for floating point numbers
 * - UTF-8 for strings
 * - Little-endian byte order
 * - Self-describing (no schema required)
 */

import { UUID, isUUID, Decimal128, isDecimal128 } from "./mod.ts";

/**
 * Type bytes for kJSONB format
 */
export enum KJsonBType {
  // Null and boolean (0x00-0x0F)
  NULL = 0x00,
  FALSE = 0x01,
  TRUE = 0x02,
  
  // Numbers (0x10-0x1F)
  INT8 = 0x10,      // -128 to 127
  INT16 = 0x11,     // -32768 to 32767
  INT32 = 0x12,     // -2147483648 to 2147483647
  INT64 = 0x13,     // -9223372036854775808 to 9223372036854775807
  UINT64 = 0x14,    // 0 to 18446744073709551615
  FLOAT32 = 0x15,   // IEEE 754 single precision
  FLOAT64 = 0x16,   // IEEE 754 double precision
  BIGINT = 0x17,    // Arbitrary precision integer
  DECIMAL128 = 0x18, // 128-bit decimal
  
  // Strings and binary (0x20-0x2F)
  STRING = 0x20,    // UTF-8 string
  BINARY = 0x21,    // Raw bytes
  
  // Temporal types (0x30-0x3F)
  DATE = 0x30,      // Date/time as milliseconds since epoch
  UUID = 0x31,      // 16-byte UUID
  
  // Container types (0x40-0x4F)
  ARRAY = 0x40,     // Array of values
  OBJECT = 0x41,    // Object with string keys
  
  // Special values (0xF0-0xFF)
  UNDEFINED = 0xF0, // JavaScript undefined
  
  // End markers
  END = 0xFF,       // End of container
}

/**
 * Options for binary encoding
 */
export interface KJsonBEncodeOptions {
  /** Use compression for strings longer than this (0 = disabled) */
  compressionThreshold?: number;
}

/**
 * Options for binary decoding
 */
export interface KJsonBDecodeOptions {
  /** Maximum depth for nested objects/arrays */
  maxDepth?: number;
}

/**
 * Binary encoder for kJSON values
 */
export class KJsonBEncoder {
  private buffer: Uint8Array;
  private position: number;
  private textEncoder: TextEncoder;
  
  constructor(initialSize = 1024) {
    this.buffer = new Uint8Array(initialSize);
    this.position = 0;
    this.textEncoder = new TextEncoder();
  }
  
  /**
   * Encode a value to binary format
   */
  encode(value: unknown): Uint8Array {
    this.position = 0;
    this.writeValue(value);
    return this.buffer.slice(0, this.position);
  }
  
  private writeValue(value: unknown): void {
    if (value === null) {
      this.writeByte(KJsonBType.NULL);
    } else if (value === undefined) {
      this.writeByte(KJsonBType.UNDEFINED);
    } else if (typeof value === "boolean") {
      this.writeByte(value ? KJsonBType.TRUE : KJsonBType.FALSE);
    } else if (typeof value === "number") {
      this.writeNumber(value);
    } else if (typeof value === "bigint") {
      this.writeBigInt(value);
    } else if (typeof value === "string") {
      this.writeString(value);
    } else if (value instanceof Date) {
      this.writeDate(value);
    } else if (isUUID(value)) {
      this.writeUUID(value);
    } else if (isDecimal128(value)) {
      this.writeDecimal128(value);
    } else if (value instanceof Uint8Array) {
      this.writeBinary(value);
    } else if (Array.isArray(value)) {
      this.writeArray(value);
    } else if (typeof value === "object") {
      this.writeObject(value as Record<string, unknown>);
    } else {
      throw new Error(`Cannot encode value of type ${typeof value}`);
    }
  }
  
  private writeByte(byte: number): void {
    this.ensureCapacity(1);
    this.buffer[this.position++] = byte;
  }
  
  private writeBytes(bytes: Uint8Array): void {
    this.ensureCapacity(bytes.length);
    this.buffer.set(bytes, this.position);
    this.position += bytes.length;
  }
  
  private writeNumber(value: number): void {
    if (!isFinite(value)) {
      // Encode NaN and Infinity as null
      this.writeByte(KJsonBType.NULL);
      return;
    }
    
    // Try to use the smallest integer representation
    if (Number.isInteger(value)) {
      if (value >= -128 && value <= 127) {
        this.writeByte(KJsonBType.INT8);
        this.writeInt8(value);
      } else if (value >= -32768 && value <= 32767) {
        this.writeByte(KJsonBType.INT16);
        this.writeInt16(value);
      } else if (value >= -2147483648 && value <= 2147483647) {
        this.writeByte(KJsonBType.INT32);
        this.writeInt32(value);
      } else {
        // Use float64 for large integers
        this.writeByte(KJsonBType.FLOAT64);
        this.writeFloat64(value);
      }
    } else {
      // Use float64 for decimals
      this.writeByte(KJsonBType.FLOAT64);
      this.writeFloat64(value);
    }
  }
  
  private writeBigInt(value: bigint): void {
    this.writeByte(KJsonBType.BIGINT);
    const str = value.toString();
    const isNegative = str[0] === '-';
    const digits = isNegative ? str.slice(1) : str;
    
    // Write sign bit and length
    const bytes = this.textEncoder.encode(digits);
    this.writeVarint((bytes.length << 1) | (isNegative ? 1 : 0));
    this.writeBytes(bytes);
  }
  
  private writeString(value: string): void {
    this.writeByte(KJsonBType.STRING);
    const bytes = this.textEncoder.encode(value);
    this.writeVarint(bytes.length);
    this.writeBytes(bytes);
  }
  
  private writeDate(value: Date): void {
    this.writeByte(KJsonBType.DATE);
    this.writeInt64(BigInt(value.getTime()));
  }
  
  private writeUUID(value: UUID): void {
    this.writeByte(KJsonBType.UUID);
    this.writeBytes(value.bytes());
  }
  
  private writeDecimal128(value: Decimal128): void {
    this.writeByte(KJsonBType.DECIMAL128);
    // For now, store as string. In production, use proper decimal encoding
    const str = value.toString();
    const bytes = this.textEncoder.encode(str);
    this.writeVarint(bytes.length);
    this.writeBytes(bytes);
  }
  
  private writeBinary(value: Uint8Array): void {
    this.writeByte(KJsonBType.BINARY);
    this.writeVarint(value.length);
    this.writeBytes(value);
  }
  
  private writeArray(value: unknown[]): void {
    this.writeByte(KJsonBType.ARRAY);
    this.writeVarint(value.length);
    for (const item of value) {
      this.writeValue(item);
    }
  }
  
  private writeObject(value: Record<string, unknown>): void {
    this.writeByte(KJsonBType.OBJECT);
    const entries = Object.entries(value);
    this.writeVarint(entries.length);
    
    for (const [key, val] of entries) {
      // Write key as string without type byte
      const keyBytes = this.textEncoder.encode(key);
      this.writeVarint(keyBytes.length);
      this.writeBytes(keyBytes);
      // Write value
      this.writeValue(val);
    }
  }
  
  private writeVarint(value: number): void {
    while (value >= 0x80) {
      this.writeByte((value & 0x7F) | 0x80);
      value >>>= 7;
    }
    this.writeByte(value & 0x7F);
  }
  
  private writeInt8(value: number): void {
    this.ensureCapacity(1);
    const view = new DataView(this.buffer.buffer, this.buffer.byteOffset);
    view.setInt8(this.position, value);
    this.position += 1;
  }
  
  private writeInt16(value: number): void {
    this.ensureCapacity(2);
    const view = new DataView(this.buffer.buffer, this.buffer.byteOffset);
    view.setInt16(this.position, value, true); // little-endian
    this.position += 2;
  }
  
  private writeInt32(value: number): void {
    this.ensureCapacity(4);
    const view = new DataView(this.buffer.buffer, this.buffer.byteOffset);
    view.setInt32(this.position, value, true); // little-endian
    this.position += 4;
  }
  
  private writeInt64(value: bigint): void {
    this.ensureCapacity(8);
    const view = new DataView(this.buffer.buffer, this.buffer.byteOffset);
    view.setBigInt64(this.position, value, true); // little-endian
    this.position += 8;
  }
  
  private writeFloat64(value: number): void {
    this.ensureCapacity(8);
    const view = new DataView(this.buffer.buffer, this.buffer.byteOffset);
    view.setFloat64(this.position, value, true); // little-endian
    this.position += 8;
  }
  
  private ensureCapacity(needed: number): void {
    const required = this.position + needed;
    if (required > this.buffer.length) {
      const newSize = Math.max(required, this.buffer.length * 2);
      const newBuffer = new Uint8Array(newSize);
      newBuffer.set(this.buffer);
      this.buffer = newBuffer;
    }
  }
}

/**
 * Binary decoder for kJSON values
 */
export class KJsonBDecoder {
  private buffer: Uint8Array;
  private position: number;
  private textDecoder: TextDecoder;
  private view: DataView;
  
  constructor() {
    this.buffer = new Uint8Array(0);
    this.position = 0;
    this.textDecoder = new TextDecoder();
    this.view = new DataView(new ArrayBuffer(0));
  }
  
  /**
   * Decode a binary buffer to a value
   */
  decode(buffer: Uint8Array): unknown {
    this.buffer = buffer;
    this.position = 0;
    this.view = new DataView(buffer.buffer, buffer.byteOffset, buffer.byteLength);
    return this.readValue();
  }
  
  private readValue(): unknown {
    if (this.position >= this.buffer.length) {
      throw new Error("Unexpected end of buffer");
    }
    
    const type = this.readByte();
    
    switch (type) {
      case KJsonBType.NULL:
        return null;
      case KJsonBType.FALSE:
        return false;
      case KJsonBType.TRUE:
        return true;
      case KJsonBType.UNDEFINED:
        return undefined;
      
      case KJsonBType.INT8:
        return this.readInt8();
      case KJsonBType.INT16:
        return this.readInt16();
      case KJsonBType.INT32:
        return this.readInt32();
      case KJsonBType.INT64:
        return Number(this.readInt64());
      case KJsonBType.FLOAT32:
        return this.readFloat32();
      case KJsonBType.FLOAT64:
        return this.readFloat64();
      case KJsonBType.BIGINT:
        return this.readBigInt();
      case KJsonBType.DECIMAL128:
        return this.readDecimal128();
      
      case KJsonBType.STRING:
        return this.readString();
      case KJsonBType.BINARY:
        return this.readBinary();
      
      case KJsonBType.DATE:
        return this.readDate();
      case KJsonBType.UUID:
        return this.readUUID();
      
      case KJsonBType.ARRAY:
        return this.readArray();
      case KJsonBType.OBJECT:
        return this.readObject();
      
      default:
        throw new Error(`Unknown type byte: 0x${type.toString(16)}`);
    }
  }
  
  private readByte(): number {
    if (this.position >= this.buffer.length) {
      throw new Error("Unexpected end of buffer");
    }
    return this.buffer[this.position++];
  }
  
  private readBytes(length: number): Uint8Array {
    if (this.position + length > this.buffer.length) {
      throw new Error("Unexpected end of buffer");
    }
    const bytes = this.buffer.slice(this.position, this.position + length);
    this.position += length;
    return bytes;
  }
  
  private readVarint(): number {
    let value = 0;
    let shift = 0;
    
    while (true) {
      const byte = this.readByte();
      value |= (byte & 0x7F) << shift;
      if ((byte & 0x80) === 0) break;
      shift += 7;
      if (shift >= 35) throw new Error("Varint too large");
    }
    
    return value;
  }
  
  private readInt8(): number {
    const value = this.view.getInt8(this.position);
    this.position += 1;
    return value;
  }
  
  private readInt16(): number {
    const value = this.view.getInt16(this.position, true); // little-endian
    this.position += 2;
    return value;
  }
  
  private readInt32(): number {
    const value = this.view.getInt32(this.position, true); // little-endian
    this.position += 4;
    return value;
  }
  
  private readInt64(): bigint {
    const value = this.view.getBigInt64(this.position, true); // little-endian
    this.position += 8;
    return value;
  }
  
  private readFloat32(): number {
    const value = this.view.getFloat32(this.position, true); // little-endian
    this.position += 4;
    return value;
  }
  
  private readFloat64(): number {
    const value = this.view.getFloat64(this.position, true); // little-endian
    this.position += 8;
    return value;
  }
  
  private readBigInt(): bigint {
    const encoded = this.readVarint();
    const length = encoded >>> 1;
    const isNegative = (encoded & 1) === 1;
    
    const bytes = this.readBytes(length);
    const digits = this.textDecoder.decode(bytes);
    const str = isNegative ? '-' + digits : digits;
    return BigInt(str);
  }
  
  private readString(): string {
    const length = this.readVarint();
    const bytes = this.readBytes(length);
    return this.textDecoder.decode(bytes);
  }
  
  private readBinary(): Uint8Array {
    const length = this.readVarint();
    return this.readBytes(length);
  }
  
  private readDate(): Date {
    const timestamp = this.readInt64();
    return new Date(Number(timestamp));
  }
  
  private readUUID(): UUID {
    const bytes = this.readBytes(16);
    return UUID.fromBytes(bytes);
  }
  
  private readDecimal128(): Decimal128 {
    // For now, read as string. In production, use proper decimal encoding
    const length = this.readVarint();
    const bytes = this.readBytes(length);
    const str = this.textDecoder.decode(bytes);
    return new Decimal128(str);
  }
  
  private readArray(): unknown[] {
    const length = this.readVarint();
    const array: unknown[] = [];
    
    for (let i = 0; i < length; i++) {
      array.push(this.readValue());
    }
    
    return array;
  }
  
  private readObject(): Record<string, unknown> {
    const length = this.readVarint();
    const obj: Record<string, unknown> = {};
    
    for (let i = 0; i < length; i++) {
      // Read key
      const keyLength = this.readVarint();
      const keyBytes = this.readBytes(keyLength);
      const key = this.textDecoder.decode(keyBytes);
      
      // Read value
      obj[key] = this.readValue();
    }
    
    return obj;
  }
}

/**
 * Encode a value to kJSONB binary format
 */
export function encode(value: unknown, options?: KJsonBEncodeOptions): Uint8Array {
  const encoder = new KJsonBEncoder();
  return encoder.encode(value);
}

/**
 * Decode a kJSONB binary buffer to a value
 */
export function decode(buffer: Uint8Array, options?: KJsonBDecodeOptions): unknown {
  const decoder = new KJsonBDecoder();
  return decoder.decode(buffer);
}

/**
 * Get the type of a value in binary format
 */
export function getType(buffer: Uint8Array): KJsonBType {
  if (buffer.length === 0) {
    throw new Error("Empty buffer");
  }
  return buffer[0];
}

/**
 * Check if a buffer is valid kJSONB format
 */
export function isValid(buffer: Uint8Array): boolean {
  try {
    decode(buffer);
    return true;
  } catch {
    return false;
  }
}