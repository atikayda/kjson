/*
 * kjson.h - kJSON C Library
 * 
 * A C implementation of the kJSON format supporting:
 * - JSON5 syntax (comments, unquoted keys, trailing commas)
 * - Extended types (BigInt, UUID, Decimal128, Date)
 * - Binary format (kJSONB)
 * - Streaming parser
 * - Zero-copy parsing where possible
 */

#ifndef KJSON_H
#define KJSON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version information */
#define KJSON_VERSION_MAJOR 1
#define KJSON_VERSION_MINOR 0
#define KJSON_VERSION_PATCH 0

/* Configuration macros */
#ifndef KJSON_MALLOC
#define KJSON_MALLOC malloc
#endif

#ifndef KJSON_FREE
#define KJSON_FREE free
#endif

#ifndef KJSON_REALLOC
#define KJSON_REALLOC realloc
#endif

/* Forward declarations */
typedef struct kjson_value kjson_value;
typedef struct kjson_parser kjson_parser;
typedef struct kjson_writer kjson_writer;

/* Error codes */
typedef enum {
    KJSON_OK = 0,
    KJSON_ERROR_MEMORY,
    KJSON_ERROR_SYNTAX,
    KJSON_ERROR_UNEXPECTED_TOKEN,
    KJSON_ERROR_INVALID_NUMBER,
    KJSON_ERROR_INVALID_STRING,
    KJSON_ERROR_INVALID_UUID,
    KJSON_ERROR_INVALID_DATE,
    KJSON_ERROR_INVALID_ESCAPE,
    KJSON_ERROR_DEPTH_EXCEEDED,
    KJSON_ERROR_SIZE_EXCEEDED,
    KJSON_ERROR_INVALID_UTF8,
    KJSON_ERROR_TRAILING_DATA,
    KJSON_ERROR_INCOMPLETE,
    KJSON_ERROR_UNSUPPORTED_TYPE,
    KJSON_ERROR_OVERFLOW,
    KJSON_ERROR_INVALID_BINARY
} kjson_error;

/* Value types */
typedef enum {
    KJSON_TYPE_NULL,
    KJSON_TYPE_BOOLEAN,
    KJSON_TYPE_NUMBER,
    KJSON_TYPE_BIGINT,
    KJSON_TYPE_DECIMAL128,
    KJSON_TYPE_STRING,
    KJSON_TYPE_UUID,
    KJSON_TYPE_INSTANT,
    KJSON_TYPE_DURATION,
    KJSON_TYPE_ARRAY,
    KJSON_TYPE_OBJECT,
    KJSON_TYPE_UNDEFINED,
    KJSON_TYPE_BINARY
} kjson_type;

/* Binary format type bytes */
typedef enum {
    KJSONB_NULL = 0x00,
    KJSONB_FALSE = 0x01,
    KJSONB_TRUE = 0x02,
    KJSONB_INT8 = 0x10,
    KJSONB_INT16 = 0x11,
    KJSONB_INT32 = 0x12,
    KJSONB_INT64 = 0x13,
    KJSONB_UINT64 = 0x14,
    KJSONB_FLOAT32 = 0x15,
    KJSONB_FLOAT64 = 0x16,
    KJSONB_BIGINT = 0x17,
    KJSONB_DECIMAL128 = 0x18,
    KJSONB_STRING = 0x20,
    KJSONB_BINARY = 0x21,
    KJSONB_INSTANT = 0x30,
    KJSONB_DURATION = 0x31,
    KJSONB_UUID = 0x32,
    KJSONB_ARRAY = 0x40,
    KJSONB_OBJECT = 0x41,
    KJSONB_UNDEFINED = 0xF0
} kjsonb_type;

/* UUID type (16 bytes) */
typedef struct {
    uint8_t bytes[16];
} kjson_uuid;

/* Decimal128 type (simplified - stores as string for now) */
typedef struct {
    char *digits;       /* Null-terminated digit string */
    int32_t exponent;   /* Exponent */
    bool negative;      /* Sign */
} kjson_decimal128;

/* Instant type (nanoseconds since epoch in Zulu time) */
typedef struct {
    int64_t nanoseconds;  /* Nanoseconds since Unix epoch (UTC) */
} kjson_instant;

/* Duration type (nanoseconds) */
typedef struct {
    int64_t nanoseconds;  /* Duration in nanoseconds */
} kjson_duration;

/* BigInt type (stores as string for portability) */
typedef struct {
    char *digits;       /* Null-terminated digit string */
    bool negative;      /* Sign */
} kjson_bigint;

/* Object member */
typedef struct kjson_member {
    char *key;
    kjson_value *value;
    struct kjson_member *next;
} kjson_member;

/* Value structure */
struct kjson_value {
    kjson_type type;
    union {
        /* Primitive types */
        bool boolean;
        double number;
        char *string;
        
        /* Extended types */
        kjson_bigint *bigint;
        kjson_decimal128 *decimal;
        kjson_uuid *uuid;
        kjson_instant *instant;
        kjson_duration *duration;
        uint8_t *binary;  /* For binary data */
        
        /* Container types */
        struct {
            kjson_value **items;
            size_t count;
            size_t capacity;
        } array;
        
        struct {
            kjson_member *first;
            size_t count;
        } object;
    } u;
    
    /* For binary data */
    size_t binary_size;
};

/* Parser options */
typedef struct {
    bool allow_comments;        /* Allow single-line and multi-line comments */
    bool allow_trailing_commas; /* Allow trailing commas */
    bool allow_unquoted_keys;   /* Allow unquoted object keys */
    bool parse_instants;        /* Parse ISO instants */
    bool parse_durations;       /* Parse ISO durations */
    bool strict_numbers;        /* Reject Infinity/NaN */
    
    size_t max_depth;           /* Maximum nesting depth */
    size_t max_string_length;   /* Maximum string length */
    
    /* Memory allocator */
    void *(*allocator)(size_t size);
    void (*deallocator)(void *ptr);
    void *allocator_ctx;
} kjson_options;

/* Parser callbacks for SAX-style parsing */
typedef struct {
    void *ctx;  /* User context */
    
    /* Value callbacks */
    int (*on_null)(void *ctx);
    int (*on_boolean)(void *ctx, bool value);
    int (*on_number)(void *ctx, double value);
    int (*on_bigint)(void *ctx, const char *digits, bool negative);
    int (*on_decimal128)(void *ctx, const char *digits, int32_t exp, bool negative);
    int (*on_string)(void *ctx, const char *str, size_t len);
    int (*on_uuid)(void *ctx, const kjson_uuid *uuid);
    int (*on_instant)(void *ctx, int64_t nanoseconds);
    int (*on_duration)(void *ctx, int64_t nanoseconds);
    
    /* Container callbacks */
    int (*on_array_start)(void *ctx);
    int (*on_array_end)(void *ctx);
    int (*on_object_start)(void *ctx);
    int (*on_object_key)(void *ctx, const char *key, size_t len);
    int (*on_object_end)(void *ctx);
} kjson_callbacks;

/* Writer options */
typedef struct {
    bool pretty;            /* Pretty print output */
    int indent;             /* Indent size (spaces) */
    bool quote_keys;        /* Always quote object keys */
    bool bigint_suffix;     /* Add 'n' suffix to BigInt */
    bool decimal_suffix;    /* Add 'm' suffix to Decimal128 */
    bool escape_unicode;    /* Escape non-ASCII as \uXXXX */
} kjson_write_options;

/* === Core API === */

/* Parse kJSON text to value tree */
kjson_value *kjson_parse(const char *text, size_t length, kjson_error *error);
kjson_value *kjson_parse_ex(const char *text, size_t length, 
                            const kjson_options *options, kjson_error *error);

/* Free a value tree */
void kjson_free(kjson_value *value);

/* Stringify a value tree */
char *kjson_stringify(const kjson_value *value);
char *kjson_stringify_ex(const kjson_value *value, const kjson_write_options *options);

/* Get string length for stringify (without allocating) */
size_t kjson_stringify_length(const kjson_value *value);
size_t kjson_stringify_length_ex(const kjson_value *value, const kjson_write_options *options);

/* === SAX Parser API === */

/* Create/destroy parser */
kjson_parser *kjson_parser_create(const kjson_callbacks *callbacks, void *ctx);
kjson_parser *kjson_parser_create_ex(const kjson_callbacks *callbacks, void *ctx,
                                     const kjson_options *options);
void kjson_parser_destroy(kjson_parser *parser);

/* Feed data to parser */
kjson_error kjson_parser_feed(kjson_parser *parser, const char *data, size_t length);
kjson_error kjson_parser_finish(kjson_parser *parser);

/* Get error details */
const char *kjson_parser_error_message(const kjson_parser *parser);
size_t kjson_parser_error_line(const kjson_parser *parser);
size_t kjson_parser_error_column(const kjson_parser *parser);

/* === Writer API === */

/* Create/destroy writer */
kjson_writer *kjson_writer_create(char *buffer, size_t size);
kjson_writer *kjson_writer_create_ex(char *buffer, size_t size, 
                                     const kjson_write_options *options);
void kjson_writer_destroy(kjson_writer *writer);

/* Write values */
kjson_error kjson_write_null(kjson_writer *writer);
kjson_error kjson_write_boolean(kjson_writer *writer, bool value);
kjson_error kjson_write_number(kjson_writer *writer, double value);
kjson_error kjson_write_bigint(kjson_writer *writer, const char *digits, bool negative);
kjson_error kjson_write_decimal128(kjson_writer *writer, const char *digits, int32_t exp, bool negative);
kjson_error kjson_write_string(kjson_writer *writer, const char *str, size_t len);
kjson_error kjson_write_uuid(kjson_writer *writer, const kjson_uuid *uuid);
kjson_error kjson_write_instant(kjson_writer *writer, int64_t nanoseconds);
kjson_error kjson_write_duration(kjson_writer *writer, int64_t nanoseconds);

/* Write containers */
kjson_error kjson_write_array_start(kjson_writer *writer);
kjson_error kjson_write_array_end(kjson_writer *writer);
kjson_error kjson_write_object_start(kjson_writer *writer);
kjson_error kjson_write_object_key(kjson_writer *writer, const char *key, size_t len);
kjson_error kjson_write_object_end(kjson_writer *writer);

/* Get result */
size_t kjson_writer_length(const kjson_writer *writer);
const char *kjson_writer_data(const kjson_writer *writer);

/* === Binary Format API === */

/* Encode to binary */
uint8_t *kjson_encode_binary(const kjson_value *value, size_t *size);
kjson_error kjson_encode_binary_to(const kjson_value *value, uint8_t *buffer, 
                                   size_t buffer_size, size_t *written);

/* Decode from binary */
kjson_value *kjson_decode_binary(const uint8_t *data, size_t size, kjson_error *error);

/* Get binary size without encoding */
size_t kjson_binary_size(const kjson_value *value);

/* === Utility Functions === */

/* Value construction helpers */
kjson_value *kjson_create_null(void);
kjson_value *kjson_create_boolean(bool value);
kjson_value *kjson_create_number(double value);
kjson_value *kjson_create_bigint(const char *digits, bool negative);
kjson_value *kjson_create_decimal128(const char *digits, int32_t exp, bool negative);
kjson_value *kjson_create_string(const char *str, size_t len);
kjson_value *kjson_create_uuid(const kjson_uuid *uuid);
kjson_value *kjson_create_instant(int64_t nanoseconds);
kjson_value *kjson_create_duration(int64_t nanoseconds);
kjson_value *kjson_create_array(void);
kjson_value *kjson_create_object(void);

/* Array operations */
kjson_error kjson_array_append(kjson_value *array, kjson_value *value);
kjson_value *kjson_array_get(const kjson_value *array, size_t index);
size_t kjson_array_size(const kjson_value *array);

/* Object operations */
kjson_error kjson_object_set(kjson_value *object, const char *key, kjson_value *value);
kjson_value *kjson_object_get(const kjson_value *object, const char *key);
bool kjson_object_has(const kjson_value *object, const char *key);
size_t kjson_object_size(const kjson_value *object);

/* Type checking */
kjson_type kjson_get_type(const kjson_value *value);
bool kjson_is_null(const kjson_value *value);
bool kjson_is_boolean(const kjson_value *value);
bool kjson_is_number(const kjson_value *value);
bool kjson_is_bigint(const kjson_value *value);
bool kjson_is_decimal128(const kjson_value *value);
bool kjson_is_string(const kjson_value *value);
bool kjson_is_uuid(const kjson_value *value);
bool kjson_is_instant(const kjson_value *value);
bool kjson_is_duration(const kjson_value *value);
bool kjson_is_array(const kjson_value *value);
bool kjson_is_object(const kjson_value *value);

/* Value extraction */
bool kjson_get_boolean(const kjson_value *value);
double kjson_get_number(const kjson_value *value);
const char *kjson_get_string(const kjson_value *value);
const kjson_bigint *kjson_get_bigint(const kjson_value *value);
const kjson_decimal128 *kjson_get_decimal128(const kjson_value *value);
const kjson_uuid *kjson_get_uuid(const kjson_value *value);
const kjson_instant *kjson_get_instant(const kjson_value *value);
const kjson_duration *kjson_get_duration(const kjson_value *value);

/* UUID utilities */
kjson_uuid kjson_uuid_from_string(const char *str);
char *kjson_uuid_to_string(const kjson_uuid *uuid);
kjson_uuid kjson_uuid_v4(void);
kjson_uuid kjson_uuid_v7(void);

/* Error handling */
const char *kjson_error_string(kjson_error error);

/* Version */
const char *kjson_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* KJSON_H */