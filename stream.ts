/**
 * kJSON Streaming Support
 * 
 * Provides streaming parser and stringifier for newline-delimited kJSON (NDJSON/JSONL style)
 * Perfect for handling large datasets, log files, and real-time data streams
 */

import { parse, stringify, KJsonParseOptions, KJsonStringifyOptions } from "./mod.ts";

/**
 * Options for streaming kJSON parser
 */
export interface KJsonStreamParseOptions extends KJsonParseOptions {
  /** Skip lines that fail to parse instead of throwing */
  skipInvalid?: boolean;
  /** Callback for handling parse errors */
  onError?: (error: Error, line: string, lineNumber: number) => void;
  /** Maximum line length in bytes (default: 1MB) */
  maxLineLength?: number;
}

/**
 * Options for streaming kJSON stringifier
 */
export interface KJsonStreamStringifyOptions extends KJsonStringifyOptions {
  /** Delimiter between objects (default: '\n') */
  delimiter?: string;
  /** Skip values that fail to stringify */
  skipInvalid?: boolean;
  /** Callback for handling stringify errors */
  onError?: (error: Error, value: unknown, index: number) => void;
}

/**
 * Parse a stream of newline-delimited kJSON
 * 
 * @example
 * ```ts
 * const file = await Deno.open("data.kjsonl");
 * const stream = parseStream(file.readable);
 * 
 * for await (const obj of stream) {
 *   console.log(obj);
 * }
 * ```
 */
export async function* parseStream(
  stream: ReadableStream<Uint8Array>,
  options: KJsonStreamParseOptions = {}
): AsyncGenerator<unknown, void, unknown> {
  const {
    skipInvalid = false,
    onError,
    maxLineLength = 1024 * 1024, // 1MB default
    ...parseOptions
  } = options;

  const decoder = new TextDecoder();
  const reader = stream.getReader();
  let buffer = "";
  let lineNumber = 0;

  try {
    while (true) {
      const { done, value } = await reader.read();
      
      if (value) {
        buffer += decoder.decode(value, { stream: true });
      }

      // Process complete lines
      let newlineIndex: number;
      while ((newlineIndex = buffer.indexOf('\n')) !== -1) {
        lineNumber++;
        const line = buffer.slice(0, newlineIndex);
        buffer = buffer.slice(newlineIndex + 1);

        // Skip empty lines
        if (line.trim() === "") {
          continue;
        }

        // Check line length
        if (line.length > maxLineLength) {
          const error = new Error(`Line ${lineNumber} exceeds maximum length of ${maxLineLength} bytes`);
          if (skipInvalid) {
            onError?.(error, line, lineNumber);
            continue;
          } else {
            throw error;
          }
        }

        try {
          const parsed = parse(line, parseOptions);
          yield parsed;
        } catch (error) {
          if (skipInvalid) {
            onError?.(error as Error, line, lineNumber);
          } else {
            throw new Error(`Parse error on line ${lineNumber}: ${error instanceof Error ? error.message : error}`);
          }
        }
      }

      if (done) {
        // Process any remaining data
        if (buffer.trim()) {
          lineNumber++;
          try {
            const parsed = parse(buffer, parseOptions);
            yield parsed;
          } catch (error) {
            if (skipInvalid) {
              onError?.(error as Error, buffer, lineNumber);
            } else {
              throw new Error(`Parse error on line ${lineNumber}: ${error instanceof Error ? error.message : error}`);
            }
          }
        }
        break;
      }

      // Check buffer size to prevent memory issues
      if (buffer.length > maxLineLength) {
        const error = new Error(`Buffer overflow: line ${lineNumber + 1} exceeds maximum length`);
        if (skipInvalid) {
          onError?.(error, buffer, lineNumber + 1);
          buffer = ""; // Clear buffer to continue
        } else {
          throw error;
        }
      }
    }
  } finally {
    reader.releaseLock();
  }
}

/**
 * Create a transform stream that parses newline-delimited kJSON
 * 
 * @example
 * ```ts
 * const response = await fetch("https://api.example.com/stream");
 * const parsed = response.body
 *   .pipeThrough(new TextDecoderStream())
 *   .pipeThrough(createParseTransform());
 * 
 * for await (const obj of parsed) {
 *   console.log(obj);
 * }
 * ```
 */
export function createParseTransform(
  options: KJsonStreamParseOptions = {}
): TransformStream<string, unknown> {
  const {
    skipInvalid = false,
    onError,
    maxLineLength = 1024 * 1024,
    ...parseOptions
  } = options;

  let buffer = "";
  let lineNumber = 0;

  return new TransformStream({
    transform(chunk: string, controller) {
      buffer += chunk;

      // Process complete lines
      let newlineIndex: number;
      while ((newlineIndex = buffer.indexOf('\n')) !== -1) {
        lineNumber++;
        const line = buffer.slice(0, newlineIndex);
        buffer = buffer.slice(newlineIndex + 1);

        // Skip empty lines
        if (line.trim() === "") {
          continue;
        }

        // Check line length
        if (line.length > maxLineLength) {
          const error = new Error(`Line ${lineNumber} exceeds maximum length of ${maxLineLength} bytes`);
          if (skipInvalid) {
            onError?.(error, line, lineNumber);
            continue;
          } else {
            controller.error(error);
            return;
          }
        }

        try {
          const parsed = parse(line, parseOptions);
          controller.enqueue(parsed);
        } catch (error) {
          if (skipInvalid) {
            onError?.(error as Error, line, lineNumber);
          } else {
            controller.error(new Error(`Parse error on line ${lineNumber}: ${error instanceof Error ? error.message : error}`));
            return;
          }
        }
      }

      // Check buffer size
      if (buffer.length > maxLineLength) {
        const error = new Error(`Buffer overflow: line ${lineNumber + 1} exceeds maximum length`);
        if (skipInvalid) {
          onError?.(error, buffer, lineNumber + 1);
          buffer = ""; // Clear buffer
        } else {
          controller.error(error);
        }
      }
    },

    flush(controller) {
      // Process any remaining data
      if (buffer.trim()) {
        lineNumber++;
        try {
          const parsed = parse(buffer, parseOptions);
          controller.enqueue(parsed);
        } catch (error) {
          if (skipInvalid) {
            onError?.(error as Error, buffer, lineNumber);
          } else {
            controller.error(new Error(`Parse error on line ${lineNumber}: ${error instanceof Error ? error.message : error}`));
          }
        }
      }
    }
  });
}

/**
 * Stringify values to a writable stream as newline-delimited kJSON
 * 
 * @example
 * ```ts
 * const file = await Deno.open("output.kjsonl", { write: true, create: true });
 * const writer = stringifyToStream(file.writable);
 * 
 * await writer.write({ id: UUID.v4(), value: 123n });
 * await writer.write({ id: UUID.v4(), value: 456n });
 * await writer.close();
 * ```
 */
export function stringifyToStream(
  stream: WritableStream<Uint8Array>,
  options: KJsonStreamStringifyOptions = {}
): KJsonStreamWriter {
  const {
    delimiter = '\n',
    skipInvalid = false,
    onError,
    ...stringifyOptions
  } = options;

  const encoder = new TextEncoder();
  const writer = stream.getWriter();
  let index = 0;
  let closed = false;

  return {
    async write(value: unknown): Promise<void> {
      if (closed) {
        throw new Error("Stream writer is closed");
      }

      try {
        const json = stringify(value, stringifyOptions);
        const data = encoder.encode(json + delimiter);
        await writer.write(data);
        index++;
      } catch (error) {
        if (skipInvalid) {
          onError?.(error as Error, value, index);
        } else {
          throw error;
        }
      }
    },

    async close(): Promise<void> {
      if (!closed) {
        closed = true;
        await writer.close();
      }
    },

    get closed(): boolean {
      return closed;
    }
  };
}

/**
 * Writer interface for streaming kJSON
 */
export interface KJsonStreamWriter {
  /** Write a value to the stream */
  write(value: unknown): Promise<void>;
  /** Close the stream */
  close(): Promise<void>;
  /** Check if the stream is closed */
  readonly closed: boolean;
}

/**
 * Create a transform stream that stringifies objects to newline-delimited kJSON
 * 
 * @example
 * ```ts
 * const data = [
 *   { id: UUID.v4(), value: 123n },
 *   { id: UUID.v4(), value: 456n }
 * ];
 * 
 * const stream = ReadableStream.from(data)
 *   .pipeThrough(createStringifyTransform())
 *   .pipeThrough(new TextEncoderStream());
 * 
 * await stream.pipeTo(file.writable);
 * ```
 */
export function createStringifyTransform(
  options: KJsonStreamStringifyOptions = {}
): TransformStream<unknown, string> {
  const {
    delimiter = '\n',
    skipInvalid = false,
    onError,
    ...stringifyOptions
  } = options;

  let index = 0;

  return new TransformStream({
    transform(value: unknown, controller) {
      try {
        const json = stringify(value, stringifyOptions);
        controller.enqueue(json + delimiter);
        index++;
      } catch (error) {
        if (skipInvalid) {
          onError?.(error as Error, value, index);
        } else {
          controller.error(error);
        }
      }
    }
  });
}

/**
 * Parse a file containing newline-delimited kJSON
 * 
 * @example
 * ```ts
 * for await (const obj of parseFile("data.kjsonl")) {
 *   console.log(obj);
 * }
 * ```
 */
export async function* parseFile(
  path: string,
  options: KJsonStreamParseOptions = {}
): AsyncGenerator<unknown, void, unknown> {
  const file = await Deno.open(path, { read: true });
  try {
    yield* parseStream(file.readable, options);
  } finally {
    // File will be closed when stream ends, but ensure cleanup
    try {
      file.close();
    } catch {
      // Ignore if already closed
    }
  }
}

/**
 * Write values to a file as newline-delimited kJSON
 * 
 * @example
 * ```ts
 * await writeFile("output.kjsonl", [
 *   { id: UUID.v4(), value: 123n },
 *   { id: UUID.v4(), value: 456n }
 * ]);
 * ```
 */
export async function writeFile(
  path: string,
  values: Iterable<unknown> | AsyncIterable<unknown>,
  options: KJsonStreamStringifyOptions = {}
): Promise<void> {
  const file = await Deno.open(path, { write: true, create: true, truncate: true });
  const writer = stringifyToStream(file.writable, options);
  
  try {
    for await (const value of values) {
      await writer.write(value);
    }
  } finally {
    await writer.close();
  }
}

/**
 * Utility to convert an async iterable to a readable stream
 * 
 * @example
 * ```ts
 * const data = async function* () {
 *   yield { id: UUID.v4(), value: 123n };
 *   yield { id: UUID.v4(), value: 456n };
 * };
 * 
 * const stream = fromAsyncIterable(data())
 *   .pipeThrough(createStringifyTransform())
 *   .pipeThrough(new TextEncoderStream());
 * ```
 */
export function fromAsyncIterable<T>(
  iterable: AsyncIterable<T>
): ReadableStream<T> {
  return new ReadableStream({
    async start(controller) {
      try {
        for await (const value of iterable) {
          controller.enqueue(value);
        }
        controller.close();
      } catch (error) {
        controller.error(error);
      }
    }
  });
}