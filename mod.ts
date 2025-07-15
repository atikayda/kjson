/**
 * kJSON - Kind JSON
 * 
 * A powerful, developer-friendly JSON parser and stringifier with extended type support for modern JavaScript/TypeScript.
 * 
 * Features:
 * - JSON5-style syntax (unquoted keys, trailing commas)
 * - JSONC comments (single-line and multi-line)
 * - BigInt values with 'n' suffix (e.g., 12345n)
 * - Instant objects as ISO timestamps with nanosecond precision (e.g., 2025-06-12T20:22:35.328456789Z)
 * - Duration objects as ISO duration strings (e.g., PT1H2M3.456S)
 * - UUID objects from unquoted UUID format (e.g., 550e8400-e29b-41d4-a716-446655440000)
 * - Decimal128 from numbers with 'm' suffix (e.g., 123.45m)
 * - Standard JSON compatibility
 * - TypeScript-first with comprehensive type safety
 * 
 * Perfect for:
 * - Blockchain applications with BigInt support
 * - Configuration files with comments
 * - APIs requiring type preservation
 * - WebSocket messages with mixed data types
 * - Financial applications with decimal precision
 * 
 * @module kJSON
 * @version 1.0.0
 * @author Kaity (Atikayda)
 * @license MIT
 */

import { UUID, isUUID } from "./uuid.ts";
import { Decimal128, isDecimal128 } from "./decimal128.ts";
import { Instant, Duration, isInstant, isDuration } from "./instant.ts";

export { UUID, isUUID, Decimal128, isDecimal128, Instant, Duration, isInstant, isDuration };

export interface KJsonParseOptions {
  /** Allow trailing commas in objects and arrays */
  allowTrailingCommas?: boolean;
  /** Allow comments in the JSON */
  allowComments?: boolean;
  /** Allow unquoted object keys */
  allowUnquotedKeys?: boolean;
  /** Parse ISO date strings as Instant objects */
  parseInstants?: boolean;
  /** Parse ISO duration strings as Duration objects */
  parseDurations?: boolean;
}

export interface KJsonStringifyOptions {
  /** Include BigInt values with 'n' suffix */
  bigintSuffix?: boolean;
  /** Convert Instant objects to ISO strings */
  serializeInstants?: boolean;
  /** Convert Duration objects to ISO strings */
  serializeDurations?: boolean;
  /** Indentation for pretty printing */
  space?: string | number;
  /** Quote object keys even if not required */
  quoteKeys?: boolean;
}

enum TokenType {
  BeginObject = "BeginObject",
  EndObject = "EndObject",
  BeginArray = "BeginArray", 
  EndArray = "EndArray",
  NameSeparator = "NameSeparator",
  ValueSeparator = "ValueSeparator",
  String = "String",
  Number = "Number",
  BigInt = "BigInt",
  Decimal = "Decimal",
  Instant = "Instant",
  Duration = "Duration",
  UUID = "UUID",
  True = "True",
  False = "False",
  Null = "Null",
  Undefined = "Undefined",
  Identifier = "Identifier",
  EOF = "EOF",
}

interface Token {
  type: TokenType;
  value: string;
  position: number;
}

class KJsonTokenizer {
  private input: string;
  private position: number = 0;
  private options: Required<KJsonParseOptions>;

  constructor(input: string, options: KJsonParseOptions = {}) {
    this.input = input;
    this.options = {
      allowTrailingCommas: options.allowTrailingCommas ?? true,
      allowComments: options.allowComments ?? true,
      allowUnquotedKeys: options.allowUnquotedKeys ?? true,
      parseInstants: options.parseInstants ?? true,
      parseDurations: options.parseDurations ?? true,
    };
  }

  *tokenize(): Generator<Token> {
    while (this.position < this.input.length) {
      this.skipWhitespaceAndComments();

      if (this.position >= this.input.length) {
        yield { type: TokenType.EOF, value: "", position: this.position };
        break;
      }

      const char = this.input[this.position];
      const position = this.position;

      switch (char) {
        case "{":
          this.position++;
          yield { type: TokenType.BeginObject, value: char, position };
          break;
        case "}":
          this.position++;
          yield { type: TokenType.EndObject, value: char, position };
          break;
        case "[":
          this.position++;
          yield { type: TokenType.BeginArray, value: char, position };
          break;
        case "]":
          this.position++;
          yield { type: TokenType.EndArray, value: char, position };
          break;
        case ":":
          this.position++;
          yield { type: TokenType.NameSeparator, value: char, position };
          break;
        case ",":
          this.position++;
          yield { type: TokenType.ValueSeparator, value: char, position };
          break;
        case '"':
        case "'":
        case '`':
          yield this.parseString();
          break;
        default:
          if (this.isDigit(char) || char === "-") {
            // Check if this might be a UUID (8 hex digits followed by dash)
            const savedPos = this.position;
            if (this.mightBeUUID()) {
              const uuid = this.tryParseCompleteUUID();
              if (uuid) {
                yield { type: TokenType.UUID, value: uuid, position: savedPos };
                break;
              }
              this.position = savedPos;
            }
            
            // Check if this might be an instant literal (starts with 4 digits and dash)
            if (this.options.parseInstants && this.looksLikeDateStart()) {
              yield this.parseInstant();
            } else {
              yield this.parseNumber();
            }
          } else if (this.isIdentifierStart(char) || char === 'P') {
            // P could be start of duration like PT1H or identifier
            if (char === 'P' && this.options.parseDurations && this.looksLikeDuration()) {
              yield this.parseDuration();
            } else {
              yield this.parseIdentifier();
            }
          } else {
            throw new Error(`Unexpected character '${char}' at position ${this.position}`);
          }
      }
    }

    yield { type: TokenType.EOF, value: "", position: this.position };
  }

  private skipWhitespaceAndComments(): void {
    while (this.position < this.input.length) {
      const char = this.input[this.position];

      // Skip whitespace
      if (/\s/.test(char)) {
        this.position++;
        continue;
      }

      // Skip comments if allowed
      if (this.options.allowComments && char === "/") {
        if (this.input[this.position + 1] === "/") {
          // Single-line comment
          this.position += 2;
          while (this.position < this.input.length && this.input[this.position] !== "\n") {
            this.position++;
          }
          continue;
        } else if (this.input[this.position + 1] === "*") {
          // Multi-line comment
          this.position += 2;
          while (this.position < this.input.length - 1) {
            if (this.input[this.position] === "*" && this.input[this.position + 1] === "/") {
              this.position += 2;
              break;
            }
            this.position++;
          }
          continue;
        }
      }

      break;
    }
  }

  private parseString(): Token {
    const quote = this.input[this.position];
    const start = this.position;
    this.position++; // Skip opening quote

    let value = "";
    while (this.position < this.input.length) {
      const char = this.input[this.position];

      if (char === quote) {
        this.position++; // Skip closing quote
        return { type: TokenType.String, value, position: start };
      }

      if (char === "\\") {
        this.position++;
        if (this.position >= this.input.length) {
          throw new Error(`Unterminated string at position ${start}`);
        }

        const escaped = this.input[this.position];
        switch (escaped) {
          case '"':
          case "'":
          case "`":
          case "\\":
          case "/":
            value += escaped;
            break;
          case "b":
            value += "\b";
            break;
          case "f":
            value += "\f";
            break;
          case "n":
            value += "\n";
            break;
          case "r":
            value += "\r";
            break;
          case "t":
            value += "\t";
            break;
          case "u": {
            // Unicode escape sequence
            if (this.position + 4 >= this.input.length) {
              throw new Error(`Invalid unicode escape at position ${this.position}`);
            }
            const hex = this.input.slice(this.position + 1, this.position + 5);
            if (!/^[0-9a-fA-F]{4}$/.test(hex)) {
              throw new Error(`Invalid unicode escape at position ${this.position}`);
            }
            value += String.fromCharCode(parseInt(hex, 16));
            this.position += 4;
            break;
          }
          default:
            throw new Error(`Invalid escape sequence '\\${escaped}' at position ${this.position}`);
        }
      } else {
        value += char;
      }

      this.position++;
    }

    throw new Error(`Unterminated string at position ${start}`);
  }

  private parseNumber(): Token {
    const start = this.position;
    let value = "";

    // Handle negative sign
    if (this.input[this.position] === "-") {
      value += this.input[this.position];
      this.position++;
    }

    // Parse digits
    while (this.position < this.input.length && this.isDigit(this.input[this.position])) {
      value += this.input[this.position];
      this.position++;
    }

    // Handle decimal point
    if (this.position < this.input.length && this.input[this.position] === ".") {
      value += this.input[this.position];
      this.position++;

      while (this.position < this.input.length && this.isDigit(this.input[this.position])) {
        value += this.input[this.position];
        this.position++;
      }
    }

    // Handle scientific notation
    if (this.position < this.input.length && /[eE]/.test(this.input[this.position])) {
      value += this.input[this.position];
      this.position++;

      if (this.position < this.input.length && /[+-]/.test(this.input[this.position])) {
        value += this.input[this.position];
        this.position++;
      }

      while (this.position < this.input.length && this.isDigit(this.input[this.position])) {
        value += this.input[this.position];
        this.position++;
      }
    }

    // Check for BigInt suffix
    if (this.position < this.input.length && this.input[this.position] === "n") {
      this.position++;
      return { type: TokenType.BigInt, value, position: start };
    }

    // Check for Decimal suffix
    if (this.position < this.input.length && this.input[this.position] === "m") {
      this.position++;
      return { type: TokenType.Decimal, value, position: start };
    }

    return { type: TokenType.Number, value, position: start };
  }

  private parseIdentifier(): Token {
    const start = this.position;
    let value = "";

    // First, try to parse as a UUID
    const savedPos = this.position;
    const uuid = this.tryParseCompleteUUID();
    if (uuid) {
      return { type: TokenType.UUID, value: uuid, position: start };
    }
    // Reset position if not a valid UUID
    this.position = savedPos;

    while (
      this.position < this.input.length &&
      (this.isIdentifierPart(this.input[this.position]))
    ) {
      value += this.input[this.position];
      this.position++;
    }

    // Check for keywords
    switch (value) {
      case "true":
        return { type: TokenType.True, value, position: start };
      case "false":
        return { type: TokenType.False, value, position: start };
      case "null":
        return { type: TokenType.Null, value, position: start };
      case "undefined":
        return { type: TokenType.Undefined, value, position: start };
      default:
        return { type: TokenType.Identifier, value, position: start };
    }
  }

  private isDigit(char: string): boolean {
    return /[0-9]/.test(char);
  }

  private isIdentifierStart(char: string): boolean {
    return /[a-zA-Z_$]/.test(char);
  }

  private isIdentifierPart(char: string): boolean {
    return /[a-zA-Z0-9_$]/.test(char);
  }

  private isISOInstantString(value: string): boolean {
    // ISO 8601 instant format: YYYY-MM-DDTHH:mm:ss.sssZ or YYYY-MM-DDTHH:mm:ss.sss+/-HH:mm
    const isoInstantRegex = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?([+-]\d{2}:\d{2}|Z)$/;
    if (!isoInstantRegex.test(value)) {
      return false;
    }
    
    try {
      Instant.from(value);
      return true;
    } catch {
      return false;
    }
  }

  private isISODurationString(value: string): boolean {
    // ISO 8601 duration format: P[nD]T[nH][nM][nS] or variations
    const isoDurationRegex = /^P(?:\d+D)?(?:T(?:\d+H)?(?:\d+M)?(?:\d+(?:\.\d+)?S)?)?$/;
    if (!isoDurationRegex.test(value)) {
      return false;
    }
    
    try {
      Duration.from(value);
      return true;
    } catch {
      return false;
    }
  }

  private looksLikeDuration(): boolean {
    // Check if the next characters look like P followed by duration components
    if (this.position + 1 >= this.input.length) return false;
    
    // Look ahead for PT or P followed by digits and D
    const peek = this.input.slice(this.position, this.position + 10);
    return /^P(?:\d+D)?T?/.test(peek);
  }

  private looksLikeDateStart(): boolean {
    // Check if the next characters look like YYYY-MM-DD
    if (this.position + 10 >= this.input.length) return false;
    
    const peek = this.input.slice(this.position, this.position + 10);
    return /^\d{4}-\d{2}-\d{2}/.test(peek);
  }

  private parseInstant(): Token {
    const start = this.position;
    let value = "";

    // Parse until we hit a delimiter (whitespace, comma, bracket, etc.)
    while (this.position < this.input.length) {
      const char = this.input[this.position];
      
      // Stop at delimiters
      if (/[\s,\]\}]/.test(char)) {
        break;
      }
      
      value += char;
      this.position++;
    }

    // Validate that this is actually a valid ISO instant
    if (this.options.parseInstants && this.isISOInstantString(value)) {
      return { type: TokenType.Instant, value, position: start };
    } else {
      // If it's not a valid instant, reset position and parse as identifier/number
      this.position = start;
      return this.parseIdentifier();
    }
  }

  private parseDuration(): Token {
    const start = this.position;
    let value = "";

    // Parse until we hit a delimiter (whitespace, comma, bracket, etc.)
    while (this.position < this.input.length) {
      const char = this.input[this.position];
      
      // Stop at delimiters
      if (/[\s,\]\}]/.test(char)) {
        break;
      }
      
      value += char;
      this.position++;
    }

    // Validate that this is actually a valid ISO duration
    if (this.options.parseDurations && this.isISODurationString(value)) {
      return { type: TokenType.Duration, value, position: start };
    } else {
      // If it's not a valid duration, reset position and parse as identifier
      this.position = start;
      return this.parseIdentifier();
    }
  }

  private mightBeUUID(): boolean {
    // Look ahead to see if this could be a UUID
    // UUIDs start with 8 hex chars followed by a dash
    const saved = this.position;
    let count = 0;
    
    while (this.position < this.input.length && count < 8) {
      const char = this.input[this.position];
      if (!/[0-9a-fA-F]/.test(char)) {
        this.position = saved;
        return false;
      }
      this.position++;
      count++;
    }
    
    const hasFirstDash = this.position < this.input.length && this.input[this.position] === '-';
    this.position = saved;
    return hasFirstDash;
  }

  private tryParseCompleteUUID(): string | null {
    const saved = this.position;
    let uuid = "";
    const parts: string[] = [];
    const expectedLengths = [8, 4, 4, 4, 12];
    
    for (let i = 0; i < expectedLengths.length; i++) {
      if (i > 0) {
        // Expect a dash between parts
        if (this.position >= this.input.length || this.input[this.position] !== '-') {
          this.position = saved;
          return null;
        }
        uuid += '-';
        this.position++;
      }
      
      let part = "";
      for (let j = 0; j < expectedLengths[i]; j++) {
        if (this.position >= this.input.length) {
          this.position = saved;
          return null;
        }
        const char = this.input[this.position];
        if (!/[0-9a-fA-F]/.test(char)) {
          this.position = saved;
          return null;
        }
        part += char;
        this.position++;
      }
      parts.push(part);
      uuid += part;
    }
    
    // Verify we're at a word boundary
    if (this.position < this.input.length) {
      const nextChar = this.input[this.position];
      if (this.isIdentifierPart(nextChar) && nextChar !== '-') {
        this.position = saved;
        return null;
      }
    }
    
    return uuid;
  }
}

class KJsonParser {
  private tokens: Token[];
  private position: number = 0;
  private options: Required<KJsonParseOptions>;

  constructor(tokens: Token[], options: KJsonParseOptions = {}) {
    this.tokens = tokens;
    this.options = {
      allowTrailingCommas: options.allowTrailingCommas ?? true,
      allowComments: options.allowComments ?? true,
      allowUnquotedKeys: options.allowUnquotedKeys ?? true,
      parseInstants: options.parseInstants ?? true,
      parseDurations: options.parseDurations ?? true,
    };
  }

  parse(): any {
    const result = this.parseValue();
    
    if (this.currentToken().type !== TokenType.EOF) {
      throw new Error(`Unexpected token after value: ${this.currentToken().type}`);
    }
    
    return result;
  }

  private parseValue(): any {
    const token = this.currentToken();

    switch (token.type) {
      case TokenType.BeginObject:
        return this.parseObject();
      case TokenType.BeginArray:
        return this.parseArray();
      case TokenType.String:
        this.advance();
        return token.value;
      case TokenType.Number:
        this.advance();
        return JSON.parse(token.value);
      case TokenType.BigInt:
        this.advance();
        return BigInt(token.value);
      case TokenType.Decimal:
        this.advance();
        return new Decimal128(token.value);
      case TokenType.Instant:
        this.advance();
        return Instant.from(token.value);
      case TokenType.Duration:
        this.advance();
        return Duration.from(token.value);
      case TokenType.UUID:
        this.advance();
        return new UUID(token.value);
      case TokenType.True:
        this.advance();
        return true;
      case TokenType.False:
        this.advance();
        return false;
      case TokenType.Null:
        this.advance();
        return null;
      case TokenType.Undefined:
        this.advance();
        return undefined;
      default:
        throw new Error(`Unexpected token: ${token.type} at position ${token.position}`);
    }
  }

  private parseObject(): Record<string, any> {
    this.expect(TokenType.BeginObject);
    const obj: Record<string, any> = {};

    if (this.currentToken().type === TokenType.EndObject) {
      this.advance();
      return obj;
    }

    while (true) {
      // Parse key
      const keyToken = this.currentToken();
      let key: string;

      if (keyToken.type === TokenType.String) {
        key = keyToken.value;
        this.advance();
      } else if (keyToken.type === TokenType.Identifier && this.options.allowUnquotedKeys) {
        key = keyToken.value;
        this.advance();
      } else {
        throw new Error(`Expected string or identifier for object key at position ${keyToken.position}`);
      }

      this.expect(TokenType.NameSeparator);

      // Parse value
      const value = this.parseValue();

      obj[key] = value;

      const nextToken = this.currentToken();
      if (nextToken.type === TokenType.EndObject) {
        this.advance();
        break;
      } else if (nextToken.type === TokenType.ValueSeparator) {
        this.advance();
        
        // Handle trailing comma
        if (this.currentToken().type === TokenType.EndObject) {
          if (this.options.allowTrailingCommas) {
            this.advance();
            break;
          } else {
            throw new Error(`Trailing comma not allowed at position ${this.currentToken().position}`);
          }
        }
      } else {
        throw new Error(`Expected ',' or '}' at position ${nextToken.position}`);
      }
    }

    return obj;
  }

  private parseArray(): any[] {
    this.expect(TokenType.BeginArray);
    const arr: any[] = [];

    if (this.currentToken().type === TokenType.EndArray) {
      this.advance();
      return arr;
    }

    while (true) {
      const value = this.parseValue();
      arr.push(value);

      const nextToken = this.currentToken();
      if (nextToken.type === TokenType.EndArray) {
        this.advance();
        break;
      } else if (nextToken.type === TokenType.ValueSeparator) {
        this.advance();
        
        // Handle trailing comma
        if (this.currentToken().type === TokenType.EndArray) {
          if (this.options.allowTrailingCommas) {
            this.advance();
            break;
          } else {
            throw new Error(`Trailing comma not allowed at position ${this.currentToken().position}`);
          }
        }
      } else {
        throw new Error(`Expected ',' or ']' at position ${nextToken.position}`);
      }
    }

    return arr;
  }

  private currentToken(): Token {
    return this.tokens[this.position] || { type: TokenType.EOF, value: "", position: -1 };
  }

  private advance(): void {
    this.position++;
  }

  private expect(expectedType: TokenType): void {
    const token = this.currentToken();
    if (token.type !== expectedType) {
      throw new Error(`Expected ${expectedType}, got ${token.type} at position ${token.position}`);
    }
    this.advance();
  }

}

/**
 * Parse a kJSON string with BigInt and Date support
 * @param text The kJSON string to parse
 * @param options Parsing options
 * @returns The parsed JavaScript value
 */
export function parse(text: string, options: KJsonParseOptions = {}): any {
  const tokenizer = new KJsonTokenizer(text, options);
  const tokens = Array.from(tokenizer.tokenize());
  const parser = new KJsonParser(tokens, options);
  return parser.parse();
}

/**
 * Convert a JavaScript value to kJSON string with BigInt and Date support
 * @param value The value to stringify
 * @param options Stringification options
 * @returns The kJSON string
 */
export function stringify(value: any, options: KJsonStringifyOptions = {}): string {
  const opts = {
    bigintSuffix: options.bigintSuffix ?? true,
    serializeInstants: options.serializeInstants ?? true,
    serializeDurations: options.serializeDurations ?? true,
    space: options.space ?? 0,
    quoteKeys: options.quoteKeys ?? false,
  };

  const indent = typeof opts.space === "number" ? " ".repeat(opts.space) : opts.space;
  let indentLevel = 0;

  function stringifyString(str: string): string {
    // Count occurrences of each quote type
    const singleQuotes = (str.match(/'/g) || []).length;
    const doubleQuotes = (str.match(/"/g) || []).length;
    const backticks = (str.match(/`/g) || []).length;
    
    // Calculate escaping cost for each quote type
    // Cost = quotes that need escaping + backslashes that need escaping
    const backslashes = (str.match(/\\/g) || []).length;
    
    const singleCost = singleQuotes + backslashes;
    const doubleCost = doubleQuotes + backslashes;
    const backtickCost = backticks + backslashes;
    
    // Find the minimum cost
    const minCost = Math.min(singleCost, doubleCost, backtickCost);
    
    // Choose quote type: in case of tie, prefer single > double > backtick
    let quoteChar: string;
    if (singleCost === minCost) {
      quoteChar = "'";
    } else if (doubleCost === minCost) {
      quoteChar = '"';
    } else {
      quoteChar = '`';
    }
    
    // Escape the string for the chosen quote type
    let escaped = str
      .replace(/\\/g, '\\\\')  // Escape backslashes first
      .replace(/\n/g, '\\n')
      .replace(/\r/g, '\\r')
      .replace(/\t/g, '\\t')
      .replace(/\b/g, '\\b')
      .replace(/\f/g, '\\f');
    
    // Escape the chosen quote character
    if (quoteChar === "'") {
      escaped = escaped.replace(/'/g, "\\\'" );
    } else if (quoteChar === '"') {
      escaped = escaped.replace(/"/g, '\\\"');
    } else { // backtick
      escaped = escaped.replace(/`/g, '\\\\`');
    }
    
    return quoteChar + escaped + quoteChar;
  }

  function stringifyValue(val: any): string {
    if (val === null) return "null";
    if (val === undefined) return "undefined";
    if (typeof val === "boolean") return val.toString();
    if (typeof val === "number") {
      if (!isFinite(val)) return "null";
      return val.toString();
    }
    if (typeof val === "bigint") {
      return opts.bigintSuffix ? `${val.toString()}n` : val.toString();
    }
    if (typeof val === "string") {
      return stringifyString(val);
    }
    if (isInstant(val)) {
      return opts.serializeInstants ? val.toString() : JSON.stringify(val.toString());
    }
    if (isDuration(val)) {
      return opts.serializeDurations ? val.toString() : JSON.stringify(val.toString());
    }
    if (isUUID(val)) {
      // Serialize UUID as unquoted string
      return val.toString();
    }
    if (isDecimal128(val)) {
      // Serialize Decimal128 with 'm' suffix
      return val.toStringWithSuffix();
    }
    if (val instanceof Date) {
      // Legacy Date support - convert to Instant
      return opts.serializeInstants ? Instant.fromEpochMilliseconds(val.getTime()).toString() : JSON.stringify(val.toISOString());
    }
    if (Array.isArray(val)) {
      return stringifyArray(val);
    }
    if (typeof val === "object") {
      return stringifyObject(val);
    }
    return "null";
  }

  function stringifyArray(arr: any[]): string {
    if (arr.length === 0) return "[]";

    indentLevel++;
    const items = arr.map(item => {
      const currentIndent = indent ? "\n" + indent.repeat(indentLevel) : "";
      const itemStr = stringifyValue(item);
      return currentIndent + itemStr;
    });
    indentLevel--;

    const result = items.join(",");
    const closeIndent = indent ? "\n" + indent.repeat(indentLevel) : "";
    return `[${result}${closeIndent}]`;
  }

  function stringifyObject(obj: Record<string, any>): string {
    const keys = Object.keys(obj);
    if (keys.length === 0) return "{}";

    indentLevel++;
    const pairs = keys.map(key => {
      const currentIndent = indent ? "\n" + indent.repeat(indentLevel) : "";
      const keyStr = shouldQuoteKey(key) || opts.quoteKeys ? JSON.stringify(key) : key;
      const valueStr = stringifyValue(obj[key]);
      const separator = indent ? ": " : ":";
      return `${currentIndent}${keyStr}${separator}${valueStr}`;
    });
    indentLevel--;

    const result = pairs.join(",");
    const closeIndent = indent ? "\n" + indent.repeat(indentLevel) : "";
    return `{${result}${closeIndent}}`;
  }

  function shouldQuoteKey(key: string): boolean {
    // Quote if it contains special characters, starts with a number, or is a reserved word
    const reserved = ['true', 'false', 'null', 'undefined'];
    return !/^[a-zA-Z_$][a-zA-Z0-9_$]*$/.test(key) || reserved.includes(key);
  }

  return stringifyValue(value);
}

/**
 * kJSON namespace with utility functions
 */
export const kJSON: {
  parse: typeof parse;
  stringify: typeof stringify;
  isValid: (text: string, options?: KJsonParseOptions) => boolean;
  safeParseWith: <T>(text: string, defaultValue: T, options?: KJsonParseOptions) => T;
  createParser: (options: KJsonParseOptions) => (text: string) => any;
  createStringifier: (options: KJsonStringifyOptions) => (value: any) => string;
} = {
  parse,
  stringify,
  
  /**
   * Check if a string is valid kJSON
   */
  isValid(text: string, options?: KJsonParseOptions): boolean {
    try {
      parse(text, options);
      return true;
    } catch {
      return false;
    }
  },

  /**
   * Parse with error handling that returns a default value
   */
  safeParseWith<T>(text: string, defaultValue: T, options?: KJsonParseOptions): T {
    try {
      return parse(text, options);
    } catch {
      return defaultValue;
    }
  },

  /**
   * Create a custom parser with preset options
   */
  createParser(options: KJsonParseOptions) {
    return (text: string) => parse(text, options);
  },

  /**
   * Create a custom stringifier with preset options
   */
  createStringifier(options: KJsonStringifyOptions) {
    return (value: any) => stringify(value, options);
  }
};

/**
 * Re-export streaming functionality
 */
export * from "./stream.ts";

/**
 * Re-export binary format functionality
 */
export * from "./binary.ts";

/**
 * Default export for convenience
 */
export default kJSON;