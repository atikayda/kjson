/**
 * kJSON - Kind JSON
 * 
 * A powerful, developer-friendly JSON parser and stringifier with extended type support for modern JavaScript/TypeScript.
 * 
 * Features:
 * - JSON5-style syntax (unquoted keys, trailing commas)
 * - JSONC comments (single-line and multi-line)
 * - BigInt values with 'n' suffix (e.g., 12345n)
 * - Date objects as ISO timestamps (e.g., 2025-06-12T20:22:35.328Z)
 * - Standard JSON compatibility
 * - TypeScript-first with comprehensive type safety
 * 
 * Perfect for:
 * - Blockchain applications with BigInt support
 * - Configuration files with comments
 * - APIs requiring type preservation
 * - WebSocket messages with mixed data types
 * 
 * @module kJSON
 * @version 1.0.0
 * @author Kaity (Atikayda)
 * @license MIT
 */

export interface KJsonParseOptions {
  /** Allow trailing commas in objects and arrays */
  allowTrailingCommas?: boolean;
  /** Allow comments in the JSON */
  allowComments?: boolean;
  /** Allow unquoted object keys */
  allowUnquotedKeys?: boolean;
  /** Parse ISO date strings as Date objects */
  parseDates?: boolean;
}

export interface KJsonStringifyOptions {
  /** Include BigInt values with 'n' suffix */
  bigintSuffix?: boolean;
  /** Convert Date objects to ISO strings */
  serializeDates?: boolean;
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
  Date = "Date",
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
      parseDates: options.parseDates ?? true,
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
          yield this.parseString();
          break;
        default:
          if (this.isDigit(char) || char === "-") {
            // Check if this might be a date literal (starts with 4 digits and dash)
            if (this.options.parseDates && this.looksLikeDateStart()) {
              yield this.parseDate();
            } else {
              yield this.parseNumber();
            }
          } else if (this.isIdentifierStart(char)) {
            yield this.parseIdentifier();
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

    return { type: TokenType.Number, value, position: start };
  }

  private parseIdentifier(): Token {
    const start = this.position;
    let value = "";

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

  private isISODateString(value: string): boolean {
    // ISO 8601 date format: YYYY-MM-DDTHH:mm:ss.sssZ or YYYY-MM-DDTHH:mm:ss.sss+/-HH:mm
    const isoDateRegex = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d{3})?([+-]\d{2}:\d{2}|Z)$/;
    return isoDateRegex.test(value) && !isNaN(Date.parse(value));
  }

  private looksLikeDateStart(): boolean {
    // Check if the next characters look like YYYY-MM-DD
    if (this.position + 10 >= this.input.length) return false;
    
    const peek = this.input.slice(this.position, this.position + 10);
    return /^\d{4}-\d{2}-\d{2}/.test(peek);
  }

  private parseDate(): Token {
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

    // Validate that this is actually a valid ISO date
    if (this.isISODateString(value)) {
      return { type: TokenType.Date, value, position: start };
    } else {
      // If it's not a valid date, reset position and parse as identifier/number
      this.position = start;
      return this.parseIdentifier();
    }
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
      parseDates: options.parseDates ?? true,
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
      case TokenType.Date:
        this.advance();
        return new Date(token.value);
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
    serializeDates: options.serializeDates ?? true,
    space: options.space ?? 0,
    quoteKeys: options.quoteKeys ?? false,
  };

  const indent = typeof opts.space === "number" ? " ".repeat(opts.space) : opts.space;
  let indentLevel = 0;

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
      return JSON.stringify(val);
    }
    if (val instanceof Date) {
      return opts.serializeDates ? val.toISOString() : JSON.stringify(val);
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

    const items = arr.map(item => {
      const currentIndent = indent ? "\n" + indent.repeat(indentLevel + 1) : "";
      const itemStr = stringifyValue(item);
      return currentIndent + itemStr;
    });

    indentLevel++;
    const result = items.join(",");
    indentLevel--;

    const closeIndent = indent ? "\n" + indent.repeat(indentLevel) : "";
    return `[${result}${closeIndent}]`;
  }

  function stringifyObject(obj: Record<string, any>): string {
    const keys = Object.keys(obj);
    if (keys.length === 0) return "{}";

    const pairs = keys.map(key => {
      const currentIndent = indent ? "\n" + indent.repeat(indentLevel + 1) : "";
      const keyStr = shouldQuoteKey(key) || opts.quoteKeys ? JSON.stringify(key) : key;
      const valueStr = stringifyValue(obj[key]);
      const separator = indent ? ": " : ":";
      return `${currentIndent}${keyStr}${separator}${valueStr}`;
    });

    indentLevel++;
    const result = pairs.join(",");
    indentLevel--;

    const closeIndent = indent ? "\n" + indent.repeat(indentLevel) : "";
    return `{${result}${closeIndent}}`;
  }

  function shouldQuoteKey(key: string): boolean {
    // Quote if it contains special characters or starts with a number
    return !/^[a-zA-Z_$][a-zA-Z0-9_$]*$/.test(key);
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
 * Default export for convenience
 */
export default kJSON;